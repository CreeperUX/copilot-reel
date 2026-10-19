#include "renderer.h"
#include "iconography.h"
#include "official_icons.h"
#include "localization.h"

#include <d2d1_1.h>
#include <d2d1effects.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <dxgi.h>
#include <DispatcherQueue.h>
#include <windows.graphics.effects.interop.h>
#include <windows.ui.composition.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Composition.Desktop.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cwchar>
#include <functional>
#include <numbers>
#include <utility>

namespace reel {
namespace {
namespace wc = winrt::Windows::UI::Composition;
namespace wf = winrt::Windows::Foundation;
namespace wn = winrt::Windows::Foundation::Numerics;
namespace we = winrt::Windows::Graphics::Effects;
namespace ae = ABI::Windows::Graphics::Effects;
namespace ac = ABI::Windows::UI::Composition;
using ac::ICompositorInterop;
using ac::ICompositionDrawingSurfaceInterop;
using ac::Desktop::ICompositorDesktopInterop;

// This adapter describes a public Direct2D effect to Windows Composition.
// No captured screenshots, undocumented DWM attributes, or Win2D dependency.
struct BackdropBlur : winrt::implements<BackdropBlur,
    we::IGraphicsEffect, we::IGraphicsEffectSource, ae::IGraphicsEffectD2D1Interop> {
    explicit BackdropBlur(we::IGraphicsEffectSource source) : source_(std::move(source)) {}
    winrt::hstring Name() const { return L"ReelBlur"; }
    void Name(winrt::hstring const&) {}
    HRESULT __stdcall GetEffectId(GUID* id) noexcept final {
        if (!id) return E_POINTER;
        *id = CLSID_D2D1GaussianBlur;
        return S_OK;
    }
    HRESULT __stdcall GetNamedPropertyMapping(LPCWSTR name, UINT* index,
        ae::GRAPHICS_EFFECT_PROPERTY_MAPPING* mapping) noexcept final {
        if (!name || !index || !mapping) return E_POINTER;
        if (std::wcscmp(name, L"BlurAmount") != 0) return E_INVALIDARG;
        *index = D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION;
        *mapping = ae::GRAPHICS_EFFECT_PROPERTY_MAPPING_DIRECT;
        return S_OK;
    }
    HRESULT __stdcall GetPropertyCount(UINT* count) noexcept final {
        if (!count) return E_POINTER;
        *count = 3;
        return S_OK;
    }
    HRESULT __stdcall GetProperty(UINT index,
        ABI::Windows::Foundation::IPropertyValue** value) noexcept final {
        if (!value) return E_POINTER;
        *value = nullptr;
        try {
            wf::IPropertyValue property{nullptr};
            switch (index) {
            case D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION:
                property = wf::PropertyValue::CreateSingle(12.0f).as<wf::IPropertyValue>();
                break;
            case D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION:
                property = wf::PropertyValue::CreateUInt32(D2D1_GAUSSIANBLUR_OPTIMIZATION_BALANCED).as<wf::IPropertyValue>();
                break;
            case D2D1_GAUSSIANBLUR_PROP_BORDER_MODE:
                property = wf::PropertyValue::CreateUInt32(D2D1_BORDER_MODE_HARD).as<wf::IPropertyValue>();
                break;
            default: return E_INVALIDARG;
            }
            *value = reinterpret_cast<ABI::Windows::Foundation::IPropertyValue*>(winrt::detach_abi(property));
            return S_OK;
        } catch (...) { return winrt::to_hresult(); }
    }
    HRESULT __stdcall GetSource(UINT index, ae::IGraphicsEffectSource** source) noexcept final {
        if (!source) return E_POINTER;
        *source = nullptr;
        if (index != 0) return E_INVALIDARG;
        try {
            source_.as<ae::IGraphicsEffectSource>().copy_to(source);
            return S_OK;
        } catch (...) { return winrt::to_hresult(); }
    }
    HRESULT __stdcall GetSourceCount(UINT* count) noexcept final {
        if (!count) return E_POINTER;
        *count = 1;
        return S_OK;
    }
private:
    we::IGraphicsEffectSource source_;
};

D2D1_COLOR_F Color(Rgba c, float alpha = -1.0f) {
    return {c.r, c.g, c.b, alpha < 0.0f ? c.a : alpha};
}

Rgba TileColor(const Appearance& appearance) {
    const float amount = appearance.dark ? 0.30f : 0.24f;
    const auto a = appearance.surface;
    const auto b = appearance.outline;
    return {a.r + (b.r - a.r) * amount, a.g + (b.g - a.g) * amount,
            a.b + (b.b - a.b) * amount, 1.0f};
}

winrt::Windows::UI::Color CompositionColor(Rgba value) {
    auto channel = [](float value) {
        return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return {channel(value.a), channel(value.r), channel(value.g), channel(value.b)};
}

std::pair<D2D1_RECT_F, D2D1_RECT_F> LanguageBounds() {
    return {D2D1::RectF(kCenterDip - 82, kCenterDip - 22, kCenterDip - 38, kCenterDip + 22),
            D2D1::RectF(kCenterDip + 38, kCenterDip - 22, kCenterDip + 82, kCenterDip + 22)};
}

// Internal actions retain the same visual weight as the official application marks.
bool DrawMenuSymbol(ID2D1RenderTarget* target, const AppTarget& item,
    const D2D1_RECT_F& bounds, ID2D1SolidColorBrush* brush, float stroke) {
    if (!item.target.starts_with(L"reel:")) return false;
    const float x = (bounds.left + bounds.right) * 0.5f;
    const float y = (bounds.top + bounds.bottom) * 0.5f;
    auto line = [&](float x1, float y1, float x2, float y2) {
        target->DrawLine({x + x1, y + y1}, {x + x2, y + y2}, brush, stroke);
    };
    auto ellipse = [&](float radius) {
        target->DrawEllipse(D2D1::Ellipse({x, y}, radius, radius), brush, stroke);
    };
    const auto& action = item.target;
    if (action == L"reel:add" || action == L"reel:manage") {
        line(-9, 0, 9, 0); line(0, -9, 0, 9);
    } else if (action == L"reel:back") {
        line(-8, 0, 9, 0); line(-8, 0, -1, -7); line(-8, 0, -1, 7);
    } else if (action == L"reel:language") {
        ellipse(10);
        target->DrawEllipse(D2D1::Ellipse({x, y}, 4.5f, 10), brush, stroke);
        line(-9, -3, 9, -3); line(-9, 3, 9, 3);
    } else if (action == L"reel:preferred") {
        ellipse(10);
        line(-4, 0, -1, 4); line(-1, 4, 6, -5);
    } else if (action == L"reel:appearance") {
        ellipse(9);
        line(0, -9, 0, 9);
        for (int ray = 0; ray != 8; ++ray) {
            const float angle = std::numbers::pi_v<float> * ray / 4;
            line(std::cos(angle) * 11, std::sin(angle) * 11,
                std::cos(angle) * 14, std::sin(angle) * 14);
        }
    } else if (action == L"reel:settings") {
        ellipse(7); ellipse(2.5f);
        for (int ray = 0; ray != 8; ++ray) {
            const float angle = std::numbers::pi_v<float> * ray / 4;
            line(std::cos(angle) * 7, std::sin(angle) * 7,
                std::cos(angle) * 10.5f, std::sin(angle) * 10.5f);
        }
    } else if (action == L"reel:remove") {
        line(-8, -7, 8, -7); line(-3, -10, 3, -10);
        line(-6, -7, -5, 10); line(-5, 10, 5, 10); line(5, 10, 6, -7);
        line(-2, -3, -2, 6); line(2, -3, 2, 6);
    } else if (action == L"reel:edit") {
        line(-8, 8, -6, 2); line(-6, 2, 5, -9); line(5, -9, 9, -5);
        line(9, -5, -2, 6); line(-2, 6, -8, 8); line(3, -7, 7, -3);
    } else if (action == L"reel:previous" || action == L"reel:next") {
        const float direction = action == L"reel:previous" ? -1.0f : 1.0f;
        line(-4 * direction, -8, 4 * direction, 0);
        line(4 * direction, 0, -4 * direction, 8);
    } else if (action == L"reel:close") {
        line(-7, -7, 7, 7); line(-7, 7, 7, -7);
    } else {
        return false;
    }
    return true;
}
} // namespace

struct OrbitRenderer::Impl {
    explicit Impl(HWND hwnd) : window(hwnd) {}
    HWND window;
    UINT dpi = 96;
    bool ready = false;
    bool visible = false;
    bool backdrop = false;
    bool softwareDevice = false;
    bool awaitCenter = false;
    bool isSubmenu = false;
    bool languageControl = false;
    unsigned surfaceDraws = 0;
    int selection = -1;
    int preferredIndex = -1;
    float anchorX = 0;
    float anchorY = 0;
    Appearance appearance;
    std::vector<AppTarget> items;
    std::wstring lastError;
    std::wstring menuTitle;
    std::wstring backdropStatus = L"solid (transparency disabled)";
    winrt::Windows::System::DispatcherQueueController queueController{nullptr};
    wc::Compositor compositor{nullptr};
    winrt::Windows::UI::Composition::Desktop::DesktopWindowTarget target{nullptr};
    wc::ContainerVisual pixelRoot{nullptr};
    wc::ContainerVisual dpiRoot{nullptr};
    wc::ContainerVisual animationRoot{nullptr};
    wc::SpriteVisual tetherVisual{nullptr};
    wc::ShapeVisual sweepVisual{nullptr};
    wc::CompositionEllipseGeometry revealGeometry{nullptr};
    wc::CompositionEllipseGeometry sweepGeometry{nullptr};
    wc::CompositionColorBrush animationBrush{nullptr};
    wc::SpriteVisual backdropVisual{nullptr};
    wc::SpriteVisual foreground{nullptr};
    wc::CompositionGraphicsDevice graphicsDevice{nullptr};
    wc::CompositionDrawingSurface surface{nullptr};
    winrt::com_ptr<ID3D11Device> d3d;
    winrt::com_ptr<ID2D1Factory1> d2dFactory;
    winrt::com_ptr<ID2D1Device> d2d;
    winrt::com_ptr<IDWriteFactory> dwrite;
    winrt::com_ptr<IDWriteTextFormat> labelFormat;
    winrt::com_ptr<IDWriteTextFormat> centerFormat;
    winrt::com_ptr<IDWriteTextFormat> hintFormat;

    void Safe(const std::function<void()>& operation) noexcept {
        try { operation(); }
        catch (const winrt::hresult_error& e) { lastError = e.message().c_str(); }
        catch (const std::exception&) { lastError = L"Renderer resource allocation failed."; }
        catch (...) { lastError = L"Unknown renderer failure."; }
    }

    void Initialize() {
        // An existing queue is borrowed. Otherwise its controller remains alive
        // for this HWND's UI-thread lifetime.
        if (!winrt::Windows::System::DispatcherQueue::GetForCurrentThread()) {
            DispatcherQueueOptions options{sizeof(DispatcherQueueOptions), DQTYPE_THREAD_CURRENT, DQTAT_COM_NONE};
            winrt::check_hresult(CreateDispatcherQueueController(options,
                reinterpret_cast<ABI::Windows::System::IDispatcherQueueController**>(winrt::put_abi(queueController))));
        }
        compositor = wc::Compositor();
        auto desktopInterop = compositor.as<ICompositorDesktopInterop>();
        winrt::check_hresult(desktopInterop->CreateDesktopWindowTarget(window, FALSE,
            reinterpret_cast<ABI::Windows::UI::Composition::Desktop::IDesktopWindowTarget**>(winrt::put_abi(target))));
        pixelRoot = compositor.CreateContainerVisual();
        dpiRoot = compositor.CreateContainerVisual();
        animationRoot = compositor.CreateContainerVisual();
        animationRoot.Size({kCanvasDip, kCanvasDip});
        animationRoot.CenterPoint({kCenterDip, kCenterDip, 0});
        revealGeometry = compositor.CreateEllipseGeometry();
        revealGeometry.Center({kCenterDip, kCenterDip});
        revealGeometry.Radius({kCenterDip, kCenterDip});
        animationRoot.Clip(compositor.CreateGeometricClip(revealGeometry));
        dpiRoot.Children().InsertAtTop(animationRoot);
        pixelRoot.Children().InsertAtTop(dpiRoot);

        // The tether and sweep are separate compositor visuals. Application marks
        // never rotate, and animation does not redraw the Direct2D surface.
        animationBrush = compositor.CreateColorBrush(CompositionColor(appearance.accent));
        tetherVisual = compositor.CreateSpriteVisual();
        tetherVisual.Brush(animationBrush);
        tetherVisual.CenterPoint({0, 0.75f, 0});
        tetherVisual.Opacity(0);
        dpiRoot.Children().InsertAtBottom(tetherVisual);
        sweepVisual = compositor.CreateShapeVisual();
        sweepVisual.Size({kCanvasDip, kCanvasDip});
        sweepVisual.CenterPoint({kCenterDip, kCenterDip, 0});
        sweepVisual.RotationAngleInDegrees(-90.0f);
        sweepVisual.Opacity(0);
        sweepGeometry = compositor.CreateEllipseGeometry();
        sweepGeometry.Center({kCenterDip, kCenterDip});
        sweepGeometry.Radius({kRadiusDip, kRadiusDip});
        auto sweepShape = compositor.CreateSpriteShape(sweepGeometry);
        sweepShape.StrokeBrush(animationBrush);
        sweepShape.StrokeThickness(1.7f);
        sweepShape.StrokeStartCap(wc::CompositionStrokeCap::Round);
        sweepShape.StrokeEndCap(wc::CompositionStrokeCap::Round);
        sweepVisual.Shapes().Append(sweepShape);
        dpiRoot.Children().InsertAtTop(sweepVisual);

        backdropVisual = compositor.CreateSpriteVisual();
        backdropVisual.Size({kRadiusDip * 2, kRadiusDip * 2});
        backdropVisual.Offset({kCenterDip - kRadiusDip, kCenterDip - kRadiusDip, 0});
        auto ellipse = compositor.CreateEllipseGeometry();
        ellipse.Center({kRadiusDip, kRadiusDip});
        ellipse.Radius({kRadiusDip, kRadiusDip});
        backdropVisual.Clip(compositor.CreateGeometricClip(ellipse));
        animationRoot.Children().InsertAtBottom(backdropVisual);
        foreground = compositor.CreateSpriteVisual();
        foreground.Size({kCanvasDip, kCanvasDip});
        animationRoot.Children().InsertAtTop(foreground);

        CreateDevices();
        winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(dwrite.put())));
        labelFormat = TextFormat(14.0f, DWRITE_FONT_WEIGHT_NORMAL);
        centerFormat = TextFormat(14.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        hintFormat = TextFormat(12.0f, DWRITE_FONT_WEIGHT_NORMAL);
        ready = true;
        ResizeSurface();
        ApplyAppearance();
        Draw();
        // Root is attached by Show. Hidden state has no active composition tree.
    }

    void CreateDevices() {
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            flags, nullptr, 0, D3D11_SDK_VERSION, d3d.put(), nullptr, nullptr);
        if (FAILED(hr)) {
            d3d = nullptr;
            softwareDevice = true;
            winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP,
                nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, d3d.put(), nullptr, nullptr));
        }
        D2D1_FACTORY_OPTIONS options{};
        winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
            __uuidof(ID2D1Factory1), &options, d2dFactory.put_void()));
        auto dxgi = d3d.as<IDXGIDevice>();
        winrt::check_hresult(d2dFactory->CreateDevice(dxgi.get(), d2d.put()));
        winrt::check_hresult(compositor.as<ICompositorInterop>()->CreateGraphicsDevice(d2d.get(),
            reinterpret_cast<ABI::Windows::UI::Composition::ICompositionGraphicsDevice**>(winrt::put_abi(graphicsDevice))));
    }

    winrt::com_ptr<IDWriteTextFormat> TextFormat(float size, DWRITE_FONT_WEIGHT weight) {
        winrt::com_ptr<IDWriteTextFormat> result;
        winrt::check_hresult(dwrite->CreateTextFormat(L"Segoe UI Variable", nullptr, weight,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"", result.put()));
        winrt::check_hresult(result->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));
        winrt::check_hresult(result->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
        winrt::check_hresult(result->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
        DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
        winrt::com_ptr<IDWriteInlineObject> ellipsis;
        winrt::check_hresult(dwrite->CreateEllipsisTrimmingSign(result.get(), ellipsis.put()));
        winrt::check_hresult(result->SetTrimming(&trimming, ellipsis.get()));
        return result;
    }

    void ResizeSurface() {
        const float scale = static_cast<float>(dpi) / 96.0f;
        const float edge = std::ceil(kCanvasDip * scale);
        pixelRoot.Size({edge, edge});
        dpiRoot.Size({kCanvasDip, kCanvasDip});
        dpiRoot.Scale({scale, scale, 1.0f});
        surface = graphicsDevice.CreateDrawingSurface({edge, edge},
            winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            winrt::Windows::Graphics::DirectX::DirectXAlphaMode::Premultiplied);
        auto brush = compositor.CreateSurfaceBrush(surface);
        brush.Stretch(wc::CompositionStretch::Fill);
        foreground.Brush(brush);
    }

    void ApplyAppearance() {
        animationBrush.Color(CompositionColor(appearance.accent));
        backdrop = false;
        backdropVisual.Brush(nullptr);
        BOOL enable = FALSE;
        DwmSetWindowAttribute(window, DWMWA_USE_HOSTBACKDROPBRUSH, &enable, sizeof(enable));
        if (!appearance.transparency || appearance.highContrast) {
            backdropStatus = appearance.highContrast ? L"solid (high contrast)" : L"solid (transparency disabled)";
        } else if (softwareDevice) {
            backdropStatus = L"solid (software rendering fallback)";
        } else {
            try {
                wc::CompositionCapabilities capabilities{nullptr};
                auto capabilityFactory = winrt::get_activation_factory<wc::CompositionCapabilities,
                    ac::Desktop::ICompositionCapabilitiesInteropFactory>();
                winrt::check_hresult(capabilityFactory->GetForWindow(window,
                    reinterpret_cast<ac::ICompositionCapabilities**>(winrt::put_abi(capabilities))));
                if (!capabilities.AreEffectsSupported() || !capabilities.AreEffectsFast())
                    winrt::throw_hresult(E_NOTIMPL);
                enable = TRUE;
                winrt::check_hresult(DwmSetWindowAttribute(window, DWMWA_USE_HOSTBACKDROPBRUSH, &enable, sizeof(enable)));
                auto source = compositor.CreateHostBackdropBrush();
                auto effect = winrt::make<BackdropBlur>(wc::CompositionEffectSourceParameter(L"Backdrop"));
                auto factory = compositor.CreateEffectFactory(effect);
                auto brush = factory.CreateBrush();
                brush.SetSourceParameter(L"Backdrop", source);
                backdropVisual.Brush(brush);
                backdrop = true;
                backdropStatus = L"host backdrop + GaussianBlur configured (visual validation required)";
            } catch (const winrt::hresult_error& e) {
                backdropVisual.Brush(nullptr);
                enable = FALSE;
                DwmSetWindowAttribute(window, DWMWA_USE_HOSTBACKDROPBRUSH, &enable, sizeof(enable));
                backdropStatus = L"solid (backdrop unavailable: " + std::wstring(e.message().c_str()) + L")";
            }
        }
        if (!appearance.animations || appearance.highContrast) {
            StopAnimations();
            ResetAnimationState();
        }
    }

    void Text(ID2D1DeviceContext* context, const std::wstring& value,
        const D2D1_RECT_F& bounds, IDWriteTextFormat* format, ID2D1Brush* brush) {
        context->DrawText(value.c_str(), static_cast<UINT32>(value.size()), format,
            bounds, brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    void DrawOnce() {
        auto interop = surface.as<ICompositionDrawingSurfaceInterop>();
        POINT offset{};
        winrt::com_ptr<ID2D1DeviceContext> context;
        winrt::check_hresult(interop->BeginDraw(nullptr, __uuidof(ID2D1DeviceContext), context.put_void(), &offset));
        // EndDraw is required after every successful BeginDraw, including errors.
        HRESULT drawResult = S_OK;
        try {
            context->SetDpi(static_cast<float>(dpi), static_cast<float>(dpi));
            const float toDip = 96.0f / static_cast<float>(dpi);
            context->SetTransform(D2D1::Matrix3x2F::Translation(offset.x * toDip, offset.y * toDip));
            context->Clear(D2D1::ColorF(0, 0.0f));
            context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            winrt::com_ptr<ID2D1SolidColorBrush> brush;
            winrt::check_hresult(context->CreateSolidColorBrush(Color(appearance.surface), brush.put()));
            const auto disk = D2D1::Ellipse(D2D1::Point2F(kCenterDip, kCenterDip), kRadiusDip, kRadiusDip);
            // Static soft contour, rendered only on state/DPI changes.
            if (!appearance.highContrast) {
                for (int spread = 12; spread >= 1; --spread) {
                    brush->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0035f * (13 - spread)));
                    auto halo = D2D1::Ellipse(D2D1::Point2F(kCenterDip, kCenterDip + 2),
                        kRadiusDip + spread * 0.8f, kRadiusDip + spread * 0.8f);
                    context->DrawEllipse(halo, brush.get(), 1.2f);
                }
            }
            brush->SetColor(Color(appearance.surface, backdrop ? (appearance.dark ? 0.83f : 0.80f) : 1.0f));
            context->FillEllipse(disk, brush.get());
            brush->SetColor(Color(appearance.outline, appearance.highContrast ? 1.0f : 0.8f));
            context->DrawEllipse(disk, brush.get(), appearance.highContrast ? 2.0f : 1.0f);

            // Quiet inner guide. Its geometry is stable during selection.
            brush->SetColor(Color(appearance.outline, appearance.highContrast ? 1.0f : 0.36f));
            context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(kCenterDip, kCenterDip),
                kDeadZoneDip, kDeadZoneDip), brush.get(), 1.0f);

            const size_t count = items.size();
            for (size_t i = 0; i < count; ++i) {
                const float angle = -std::numbers::pi_v<float> / 2.0f +
                    2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(count);
                const float x = kCenterDip + 112.0f * std::cos(angle);
                const float y = kCenterDip + 112.0f * std::sin(angle);
                const bool selected = !awaitCenter && static_cast<int>(i) == selection;
                if (selected) {
                    const auto seat = D2D1::RoundedRect(D2D1::RectF(x - 30, y - 32, x + 30, y + 32), 16, 16);
                    brush->SetColor(Color(appearance.selection, 1.0f));
                    context->FillRoundedRectangle(seat, brush.get());
                    brush->SetColor(Color(appearance.accent, 1.0f));
                    context->DrawRoundedRectangle(seat, brush.get(), appearance.highContrast ? 2.5f : 1.5f);
                }
                // Official marks share one tile, optical size, and selection
                // treatment. Internal commands use theme-aware vector strokes.
                const auto tile = D2D1::RoundedRect(D2D1::RectF(x - 20, y - 20, x + 20, y + 20), 10, 10);
                brush->SetColor(Color(appearance.highContrast
                    ? (selected ? appearance.selection : appearance.surface) : TileColor(appearance)));
                context->FillRoundedRectangle(tile, brush.get());
                if (appearance.highContrast) {
                    brush->SetColor(Color(selected ? appearance.selectionText : appearance.foreground));
                    context->DrawRoundedRectangle(tile, brush.get(), 1.0f);
                }
                brush->SetColor(Color(selected ? (appearance.highContrast
                    ? appearance.selectionText : appearance.accent) : appearance.foreground));
                const auto iconBounds = D2D1::RectF(x - 14, y - 14, x + 14, y + 14);
                const bool artworkOnDarkSurface = appearance.highContrast
                    ? appearance.surface.r * 0.2126f + appearance.surface.g * 0.7152f + appearance.surface.b * 0.0722f < 0.5f
                    : appearance.dark;
                if (!DrawMenuSymbol(context.get(), items[i], iconBounds, brush.get(),
                    appearance.highContrast ? 2.0f : 1.75f) &&
                    !DrawOfficialIcon(context.get(), items[i], iconBounds, artworkOnDarkSurface)) {
                    // Missing artwork gets a neutral application glyph, never
                    // an invented approximation of a vendor's official mark.
                    DrawAppSymbol(context.get(), AppTarget{}, iconBounds, brush.get(),
                        appearance.highContrast ? 2.0f : 1.75f);
                }
                if (static_cast<int>(i) == preferredIndex) {
                    brush->SetColor(Color(appearance.accent));
                    context->FillEllipse(D2D1::Ellipse({x + 18, y - 18}, 5.5f, 5.5f), brush.get());
                    brush->SetColor(Color(appearance.surface));
                    context->DrawLine({x + 15.5f, y - 18}, {x + 17.2f, y - 16}, brush.get(), 1.2f);
                    context->DrawLine({x + 17.2f, y - 16}, {x + 20.5f, y - 20}, brush.get(), 1.2f);
                }
                // Full names live in the centre; tiles remain icon-first and do
                // not shift or collide when the user configures eight targets.
                if (selected) {
                    brush->SetColor(Color(appearance.highContrast ? appearance.selectionText : appearance.accent));
                    context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y + 26), 2.0f, 2.0f), brush.get());
                }
                if (isSubmenu && languageControl) {
                    const wchar_t* label = nullptr;
                    if (items[i].target == L"reel:language") label = Tr(L"Language", L"语言");
                    else if (items[i].target == L"reel:preferred") label = Tr(L"Preferred", L"首选应用");
                    else if (items[i].target == L"reel:appearance") label = Tr(L"Appearance", L"外观");
                    if (label) {
                        brush->SetColor(Color(selected ? appearance.accent : appearance.secondary));
                        Text(context.get(), label, D2D1::RectF(x - 55, y + 33, x + 55, y + 51),
                            hintFormat.get(), brush.get());
                    }
                }
            }
            const bool selected = !awaitCenter && selection >= 0 && static_cast<size_t>(selection) < items.size();
            brush->SetColor(Color(appearance.foreground));
            if (languageControl && !awaitCenter) {
                Text(context.get(), CurrentLanguage() == Language::English ? L"English" : L"简体中文",
                    D2D1::RectF(kCenterDip - 38, kCenterDip - 13, kCenterDip + 38, kCenterDip + 9),
                    centerFormat.get(), brush.get());
                const auto bounds = LanguageBounds();
                for (const auto& rect : {bounds.first, bounds.second}) {
                    const float x = (rect.left + rect.right) * 0.5f;
                    const float y = (rect.top + rect.bottom) * 0.5f;
                    brush->SetColor(Color(appearance.highContrast ? appearance.surface : TileColor(appearance)));
                    context->FillRoundedRectangle(D2D1::RoundedRect(
                        D2D1::RectF(x - 15, y - 15, x + 15, y + 15), 9, 9), brush.get());
                    brush->SetColor(Color(appearance.foreground));
                    const float direction = x < kCenterDip ? -1.0f : 1.0f;
                    context->DrawLine({x - direction * 2.5f, y - 5}, {x + direction * 2.5f, y}, brush.get(), 1.6f);
                    context->DrawLine({x + direction * 2.5f, y}, {x - direction * 2.5f, y + 5}, brush.get(), 1.6f);
                }
                brush->SetColor(Color(appearance.secondary));
                Text(context.get(), Tr(L"A / D to change", L"A / D 切换"),
                    D2D1::RectF(kCenterDip - 70, kCenterDip + 11, kCenterDip + 70, kCenterDip + 29),
                    hintFormat.get(), brush.get());
                Text(context.get(), Tr(L"Language", L"语言"),
                    D2D1::RectF(kCenterDip - 60, kCenterDip - 38, kCenterDip + 60, kCenterDip - 19),
                    hintFormat.get(), brush.get());
            } else {
                const std::wstring heading = awaitCenter ? Tr(L"Move here", L"移入圆心") :
                    selected ? items[selection].name :
                    menuTitle.empty() ? Tr(L"Copilot Reel", L"Copilot Reel") : menuTitle;
                Text(context.get(), heading,
                    D2D1::RectF(kCenterDip - 66, kCenterDip - 17, kCenterDip + 66, kCenterDip + 5),
                    centerFormat.get(), brush.get());
                brush->SetColor(Color(appearance.secondary));
                std::wstring hint;
                if (awaitCenter) hint = Tr(L"then choose", L"然后选择");
                else if (selected && items[selection].target.starts_with(L"reel:"))
                    hint = Tr(L"Open menu", L"打开菜单");
                else if (selected) hint = isSubmenu ? Tr(L"Choose", L"选择") : Tr(L"Release to open", L"松开打开");
                else hint = isSubmenu ? Tr(L"Center to go back", L"点击中心返回") : Tr(L"Choose an app", L"选择应用");
                Text(context.get(), hint,
                    D2D1::RectF(kCenterDip - 74, kCenterDip + 7, kCenterDip + 74, kCenterDip + 25),
                    hintFormat.get(), brush.get());
            }
        } catch (...) { drawResult = winrt::to_hresult(); }
        HRESULT endResult = interop->EndDraw();
        winrt::check_hresult(drawResult);
        winrt::check_hresult(endResult);
        ++surfaceDraws;
    }

    void Draw() {
        try { DrawOnce(); }
        catch (const winrt::hresult_error& e) {
            const HRESULT code = e.code();
            if (code != D2DERR_RECREATE_TARGET && code != DXGI_ERROR_DEVICE_REMOVED && code != DXGI_ERROR_DEVICE_RESET)
                throw;
            // Recreate cached GPU resources once after device loss. Selection,
            // target ordering and the gesture remain owned by the controller.
            ReleaseOfficialIcons(nullptr);
            surface = nullptr;
            graphicsDevice = nullptr;
            d2d = nullptr;
            d2dFactory = nullptr;
            d3d = nullptr;
            softwareDevice = false;
            CreateDevices();
            ResizeSurface();
            ApplyAppearance();
            DrawOnce();
        }
    }

    void StopAnimations() {
        animationRoot.StopAnimation(L"Opacity");
        animationRoot.StopAnimation(L"Scale");
        revealGeometry.StopAnimation(L"Radius");
        tetherVisual.StopAnimation(L"Opacity");
        tetherVisual.StopAnimation(L"Scale");
        sweepVisual.StopAnimation(L"Opacity");
        sweepGeometry.StopAnimation(L"TrimEnd");
    }

    void ResetAnimationState() {
        animationRoot.Opacity(1.0f);
        animationRoot.Scale({1, 1, 1});
        revealGeometry.Radius({kCenterDip, kCenterDip});
        tetherVisual.Opacity(0);
        tetherVisual.Scale({1, 1, 1});
        sweepVisual.Opacity(0);
        sweepGeometry.TrimEnd(1);
    }

    void PositionTether() {
        const float length = std::hypot(anchorX, anchorY);
        tetherVisual.Size({length, 1.5f});
        tetherVisual.Offset({kCenterDip + anchorX, kCenterDip + anchorY - 0.75f, 0});
        tetherVisual.RotationAngleInDegrees(std::atan2(-anchorY, -anchorX) *
            180.0f / std::numbers::pi_v<float>);
    }

    void Show() {
        if (FAILED(d3d->GetDeviceRemovedReason())) Draw();
        target.Root(pixelRoot);
        visible = true;
        StopAnimations();
        ResetAnimationState();
        PositionTether();
        if (!appearance.animations || appearance.highContrast) return;

        constexpr unsigned duration = 230;
        const float arrival = std::hypot(anchorX, anchorY) > 2.0f ? 70.0f / duration : 0.0f;
        auto ease = compositor.CreateCubicBezierEasingFunction({0.16f, 1.0f}, {0.3f, 1.0f});
        auto linear = compositor.CreateLinearEasingFunction();

        auto tetherScale = compositor.CreateVector3KeyFrameAnimation();
        tetherScale.Duration(std::chrono::milliseconds(duration));
        tetherScale.InsertKeyFrame(0.0f, {0, 1, 1});
        tetherScale.InsertKeyFrame((std::max)(arrival, 0.01f), {1, 1, 1}, ease);
        tetherScale.InsertKeyFrame(1.0f, {1, 1, 1});
        tetherVisual.StartAnimation(L"Scale", tetherScale);
        auto tetherOpacity = compositor.CreateScalarKeyFrameAnimation();
        tetherOpacity.Duration(std::chrono::milliseconds(duration));
        tetherOpacity.InsertKeyFrame(0, 0.8f);
        tetherOpacity.InsertKeyFrame(arrival + 0.16f, 0.8f);
        tetherOpacity.InsertKeyFrame(arrival + 0.33f, 0, ease);
        tetherOpacity.InsertKeyFrame(1, 0);
        tetherVisual.StartAnimation(L"Opacity", tetherOpacity);

        auto opacity = compositor.CreateScalarKeyFrameAnimation();
        opacity.Duration(std::chrono::milliseconds(duration));
        opacity.InsertKeyFrame(0, 0);
        if (arrival > 0) opacity.InsertKeyFrame(arrival, 0);
        opacity.InsertKeyFrame(arrival + 0.22f, 1, ease);
        opacity.InsertKeyFrame(1, 1);
        animationRoot.StartAnimation(L"Opacity", opacity);
        auto scale = compositor.CreateVector3KeyFrameAnimation();
        scale.Duration(std::chrono::milliseconds(duration));
        scale.InsertKeyFrame(0, {0.85f, 0.85f, 1});
        if (arrival > 0) scale.InsertKeyFrame(arrival, {0.85f, 0.85f, 1});
        scale.InsertKeyFrame(1, {1, 1, 1}, ease);
        animationRoot.StartAnimation(L"Scale", scale);
        auto reveal = compositor.CreateVector2KeyFrameAnimation();
        reveal.Duration(std::chrono::milliseconds(duration));
        reveal.InsertKeyFrame(0, {0, 0});
        if (arrival > 0) reveal.InsertKeyFrame(arrival, {0, 0});
        reveal.InsertKeyFrame(1, {kCenterDip, kCenterDip}, ease);
        revealGeometry.StartAnimation(L"Radius", reveal);

        auto sweep = compositor.CreateScalarKeyFrameAnimation();
        sweep.Duration(std::chrono::milliseconds(duration));
        sweep.InsertKeyFrame(0, 0);
        if (arrival > 0) sweep.InsertKeyFrame(arrival, 0);
        sweep.InsertKeyFrame(0.94f, 1, linear);
        sweep.InsertKeyFrame(1, 1);
        sweepGeometry.StartAnimation(L"TrimEnd", sweep);
        auto sweepOpacity = compositor.CreateScalarKeyFrameAnimation();
        sweepOpacity.Duration(std::chrono::milliseconds(duration));
        sweepOpacity.InsertKeyFrame(0, 0);
        if (arrival > 0) sweepOpacity.InsertKeyFrame(arrival, 0);
        sweepOpacity.InsertKeyFrame(arrival + 0.05f, 0.85f);
        sweepOpacity.InsertKeyFrame(0.86f, 0.85f);
        sweepOpacity.InsertKeyFrame(1, 0, ease);
        sweepVisual.StartAnimation(L"Opacity", sweepOpacity);
    }

    unsigned BeginHide() {
        if (!visible || !appearance.animations || appearance.highContrast) {
            Hide();
            return 0;
        }
        // Replace the active entry animations without resetting their properties.
        // StartingValue then inherits the current compositor presentation value.
        PositionTether();
        constexpr unsigned duration = 190;
        auto ease = compositor.CreateCubicBezierEasingFunction({0.4f, 0}, {1, 1});
        auto linear = compositor.CreateLinearEasingFunction();
        auto opacity = compositor.CreateScalarKeyFrameAnimation();
        opacity.Duration(std::chrono::milliseconds(duration));
        // StartingValue preserves continuity when a quick release interrupts entry.
        opacity.InsertExpressionKeyFrame(0, L"this.StartingValue");
        opacity.InsertKeyFrame(0.64f, 0, ease);
        opacity.InsertKeyFrame(1, 0);
        animationRoot.StartAnimation(L"Opacity", opacity);
        auto scale = compositor.CreateVector3KeyFrameAnimation();
        scale.Duration(std::chrono::milliseconds(duration));
        scale.InsertExpressionKeyFrame(0, L"this.StartingValue");
        scale.InsertKeyFrame(0.64f, {0.85f, 0.85f, 1}, ease);
        scale.InsertKeyFrame(1, {0.85f, 0.85f, 1});
        animationRoot.StartAnimation(L"Scale", scale);
        auto reveal = compositor.CreateVector2KeyFrameAnimation();
        reveal.Duration(std::chrono::milliseconds(duration));
        reveal.InsertExpressionKeyFrame(0, L"this.StartingValue");
        reveal.InsertKeyFrame(0.64f, {0, 0}, ease);
        reveal.InsertKeyFrame(1, {0, 0});
        revealGeometry.StartAnimation(L"Radius", reveal);
        auto sweep = compositor.CreateScalarKeyFrameAnimation();
        sweep.Duration(std::chrono::milliseconds(duration));
        sweep.InsertKeyFrame(0, 1);
        sweep.InsertKeyFrame(0.64f, 0, linear);
        sweep.InsertKeyFrame(1, 0);
        sweepGeometry.StartAnimation(L"TrimEnd", sweep);
        auto sweepOpacity = compositor.CreateScalarKeyFrameAnimation();
        sweepOpacity.Duration(std::chrono::milliseconds(duration));
        sweepOpacity.InsertKeyFrame(0, 0);
        sweepOpacity.InsertKeyFrame(0.08f, 0.55f);
        sweepOpacity.InsertKeyFrame(0.56f, 0.55f);
        sweepOpacity.InsertKeyFrame(0.64f, 0);
        sweepOpacity.InsertKeyFrame(1, 0);
        sweepVisual.StartAnimation(L"Opacity", sweepOpacity);
        auto tetherOpacity = compositor.CreateScalarKeyFrameAnimation();
        tetherOpacity.Duration(std::chrono::milliseconds(duration));
        tetherOpacity.InsertKeyFrame(0, 0);
        tetherOpacity.InsertKeyFrame(0.36f, 0.8f);
        tetherOpacity.InsertKeyFrame(0.83f, 0.8f);
        tetherOpacity.InsertKeyFrame(1, 0);
        tetherVisual.StartAnimation(L"Opacity", tetherOpacity);
        auto tetherScale = compositor.CreateVector3KeyFrameAnimation();
        tetherScale.Duration(std::chrono::milliseconds(duration));
        tetherScale.InsertKeyFrame(0, {1, 1, 1});
        tetherScale.InsertKeyFrame(0.64f, {1, 1, 1});
        tetherScale.InsertKeyFrame(1, {0, 1, 1}, ease);
        tetherVisual.StartAnimation(L"Scale", tetherScale);
        return duration;
    }

    void Hide() {
        visible = false;
        StopAnimations();
        target.Root(nullptr);
    }
};

OrbitRenderer::OrbitRenderer(HWND window) : impl_(std::make_unique<Impl>(window)) {}
OrbitRenderer::~OrbitRenderer() {
    if (!impl_) return;
    impl_->Safe([&] {
        if (impl_->target) impl_->target.Root(nullptr);
        ReleaseOfficialIcons(nullptr);
        BOOL disabled = FALSE;
        DwmSetWindowAttribute(impl_->window, DWMWA_USE_HOSTBACKDROPBRUSH, &disabled, sizeof(disabled));
    });
}
bool OrbitRenderer::Initialize(std::wstring& error) {
    impl_->Safe([&] { impl_->Initialize(); });
    error = impl_->lastError;
    return impl_->ready && error.empty();
}
void OrbitRenderer::Resize(UINT dpi) {
    if (!impl_->ready || impl_->dpi == dpi || !dpi) return;
    impl_->Safe([&] { impl_->dpi = dpi; impl_->ResizeSurface(); impl_->Draw(); });
}
void OrbitRenderer::SetAppearance(const Appearance& appearance) {
    impl_->appearance = appearance;
    if (!impl_->ready) return;
    impl_->Safe([&] { impl_->ApplyAppearance(); impl_->Draw(); });
}
void OrbitRenderer::SetItems(const std::vector<AppTarget>& items) {
    impl_->Safe([&] {
        impl_->items = items;
        if (impl_->selection >= static_cast<int>(items.size())) impl_->selection = -1;
        if (impl_->ready) impl_->Draw();
    });
}
void OrbitRenderer::SetIcons(const std::vector<HICON>&) {
    // Retained for existing callers. Packaged official artwork is authoritative.
}
void OrbitRenderer::SetMenuContext(const std::wstring& title, bool isSubmenu, bool languageControl) {
    impl_->Safe([&] {
        impl_->menuTitle = title;
        impl_->isSubmenu = isSubmenu;
        impl_->languageControl = languageControl;
        if (impl_->ready) impl_->Draw();
    });
}
void OrbitRenderer::SetPreferredIndex(int index) {
    if (impl_->preferredIndex == index) return;
    impl_->preferredIndex = index;
    if (impl_->ready) impl_->Safe([&] { impl_->Draw(); });
}
void OrbitRenderer::SetAnchorOffset(float x, float y) {
    impl_->anchorX = x;
    impl_->anchorY = y;
}
std::pair<D2D1_RECT_F, D2D1_RECT_F> OrbitRenderer::LanguageArrowBounds() const {
    return LanguageBounds();
}
void OrbitRenderer::SetAwaitCenter(bool awaitCenter) {
    if (impl_->awaitCenter == awaitCenter) return;
    impl_->awaitCenter = awaitCenter;
    if (impl_->ready) impl_->Safe([&] { impl_->Draw(); });
}
void OrbitRenderer::SetSelection(int index) {
    if (index < -1 || index >= static_cast<int>(impl_->items.size())) index = -1;
    if (impl_->selection == index) return;
    impl_->selection = index;
    if (impl_->ready) impl_->Safe([&] { impl_->Draw(); });
}
void OrbitRenderer::Show() {
    if (impl_->ready) impl_->Safe([&] { impl_->Show(); });
}
unsigned OrbitRenderer::BeginHide() {
    unsigned duration = 0;
    if (impl_->ready) impl_->Safe([&] { duration = impl_->BeginHide(); });
    return duration;
}
void OrbitRenderer::Hide() {
    if (impl_->ready) impl_->Safe([&] { impl_->Hide(); });
}
bool OrbitRenderer::UsesBackdrop() const { return impl_->backdrop; }
std::wstring OrbitRenderer::Diagnostics() const {
    return L"Windows Composition + Direct2D/DirectWrite; icons=official-artwork-unified-tiles; " + impl_->backdropStatus +
        L"; center gate=" + (impl_->awaitCenter ? L"waiting" : L"armed") +
        L"; menu=" + impl_->menuTitle + L"; submenu=" + (impl_->isSubmenu ? L"yes" : L"no") +
        L"; animation=tether-70ms/reveal-160ms/exit-190ms; anchor=" + std::to_wstring(impl_->anchorX) +
        L"," + std::to_wstring(impl_->anchorY) +
        L"; DPI=" + std::to_wstring(impl_->dpi) + L"; surface draws=" + std::to_wstring(impl_->surfaceDraws) +
        L"; device=" + (impl_->softwareDevice ? L"WARP" : L"hardware") +
        (impl_->lastError.empty() ? L"" : L"; last error=" + impl_->lastError);
}
} // namespace reel
