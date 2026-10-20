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

std::pair<D2D1_RECT_F, D2D1_RECT_F> LanguageBounds() {
    // The language control lives on the top orbit tile, not in the cancel zone.
    constexpr float y = kCenterDip - 112.0f;
    return {D2D1::RectF(kCenterDip - 68, y - 20, kCenterDip - 28, y + 20),
            D2D1::RectF(kCenterDip + 28, y - 20, kCenterDip + 68, y + 20)};
}

// Internal actions retain the same visual weight as the official application marks.
bool DrawMenuSymbol(ID2D1RenderTarget* target, const AppTarget& item,
    const D2D1_RECT_F& bounds, ID2D1SolidColorBrush* brush, float stroke, bool dark) {
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
    if (action == L"reel:add") {
        line(-9, 0, 9, 0); line(0, -9, 0, 9);
    } else if (action == L"reel:manage") {
        for (const float dx : {-7.0f, 7.0f}) for (const float dy : {-7.0f, 7.0f})
            target->DrawRoundedRectangle(D2D1::RoundedRect(
                D2D1::RectF(x + dx - 4, y + dy - 4, x + dx + 4, y + dy + 4),
                1.5f, 1.5f), brush, stroke);
    } else if (action == L"reel:back") {
        line(-8, 0, 9, 0); line(-8, 0, -1, -7); line(-8, 0, -1, 7);
    } else if (action == L"reel:preferred") {
        ellipse(10);
        line(-4, 0, -1, 4); line(-1, 4, 6, -5);
    } else if (action == L"reel:appearance") {
        if (dark) {
            winrt::com_ptr<ID2D1Factory> factory;
            target->GetFactory(factory.put());
            winrt::com_ptr<ID2D1PathGeometry> moon;
            winrt::check_hresult(factory->CreatePathGeometry(moon.put()));
            winrt::com_ptr<ID2D1GeometrySink> sink;
            winrt::check_hresult(moon->Open(sink.put()));
            sink->BeginFigure({x + 3, y - 10}, D2D1_FIGURE_BEGIN_HOLLOW);
            sink->AddBezier(D2D1::BezierSegment({x - 11, y - 12}, {x - 14, y + 9}, {x - 1, y + 11}));
            sink->AddBezier(D2D1::BezierSegment({x + 4, y + 12}, {x + 9, y + 9}, {x + 11, y + 5}));
            sink->AddBezier(D2D1::BezierSegment({x - 1, y + 10}, {x - 6, y - 4}, {x + 3, y - 10}));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            winrt::check_hresult(sink->Close());
            target->DrawGeometry(moon.get(), brush, stroke);
        } else {
            ellipse(6.5f);
            for (int ray = 0; ray != 8; ++ray) {
                const float angle = std::numbers::pi_v<float> * ray / 4;
                line(std::cos(angle) * 10, std::sin(angle) * 10,
                    std::cos(angle) * 13, std::sin(angle) * 13);
            }
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
    wc::CompositionEllipseGeometry revealGeometry{nullptr};
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
        labelFormat = TextFormat(16.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        centerFormat = TextFormat(14.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
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
            brush->SetColor(Color(appearance.outline, appearance.highContrast ? 1.0f : 0.45f));
            context->DrawEllipse(disk, brush.get(), appearance.highContrast ? 2.0f : 1.0f);

            // Quiet inner guide. Its geometry is stable during selection.
            brush->SetColor(Color(appearance.outline, appearance.highContrast ? 1.0f : awaitCenter ? 0.55f : 0.18f));
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
                if (items[i].target == L"reel:language") {
                    Text(context.get(), CurrentLanguage() == Language::English ? L"EN" : L"CN",
                        iconBounds, labelFormat.get(), brush.get());
                } else if (!DrawMenuSymbol(context.get(), items[i], iconBounds, brush.get(),
                    appearance.highContrast ? 2.0f : 1.75f, appearance.dark) &&
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
            }
            if (languageControl && !awaitCenter) {
                const auto bounds = LanguageBounds();
                brush->SetColor(Color(appearance.highContrast ? appearance.foreground : appearance.secondary));
                for (const auto& rect : {bounds.first, bounds.second}) {
                    const float x = (rect.left + rect.right) * 0.5f;
                    const float y = (rect.top + rect.bottom) * 0.5f;
                    const float direction = x < kCenterDip ? -1.0f : 1.0f;
                    context->DrawLine({x - direction * 2.5f, y - 5}, {x + direction * 2.5f, y}, brush.get(), 1.6f);
                    context->DrawLine({x + direction * 2.5f, y}, {x - direction * 2.5f, y + 5}, brush.get(), 1.6f);
                }
            }
            const bool selected = !awaitCenter && selection >= 0 && static_cast<size_t>(selection) < items.size();
            const bool selectedApplication = selected && !items[selection].target.starts_with(L"reel:");
            if (selectedApplication) {
                // Only application identity belongs in the center. Commands and
                // page state are expressed by their icons without captions.
                brush->SetColor(Color(appearance.foreground));
                Text(context.get(), items[selection].name,
                    D2D1::RectF(kCenterDip - 66, kCenterDip - 13, kCenterDip + 66, kCenterDip + 13),
                    centerFormat.get(), brush.get());
            } else if (isSubmenu && !awaitCenter) {
                brush->SetColor(Color(appearance.secondary, appearance.highContrast ? 1.0f : 0.72f));
                context->DrawLine({kCenterDip + 3, kCenterDip - 6}, {kCenterDip - 3, kCenterDip}, brush.get(), 1.6f);
                context->DrawLine({kCenterDip - 3, kCenterDip}, {kCenterDip + 3, kCenterDip + 6}, brush.get(), 1.6f);
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
    }

    void ResetAnimationState() {
        animationRoot.Opacity(1.0f);
        animationRoot.Scale({1, 1, 1});
    }

    void Show() {
        if (FAILED(d3d->GetDeviceRemovedReason())) Draw();
        target.Root(pixelRoot);
        visible = true;
        StopAnimations();
        ResetAnimationState();
        if (!appearance.animations || appearance.highContrast) return;

        // A restrained settling transition keeps the edge neutral and the
        // pointer's relationship to the center stable throughout the opening.
        constexpr unsigned duration = 160;
        auto ease = compositor.CreateCubicBezierEasingFunction({0.2f, 0}, {0, 1});
        auto opacity = compositor.CreateScalarKeyFrameAnimation();
        opacity.Duration(std::chrono::milliseconds(duration));
        opacity.InsertKeyFrame(0, 0);
        opacity.InsertKeyFrame(1, 1, ease);
        animationRoot.StartAnimation(L"Opacity", opacity);
        auto scale = compositor.CreateVector3KeyFrameAnimation();
        scale.Duration(std::chrono::milliseconds(duration));
        scale.InsertKeyFrame(0, {0.96f, 0.96f, 1});
        scale.InsertKeyFrame(1, {1, 1, 1}, ease);
        animationRoot.StartAnimation(L"Scale", scale);
    }

    unsigned BeginHide() {
        if (!visible || !appearance.animations || appearance.highContrast) {
            Hide();
            return 0;
        }
        constexpr unsigned duration = 120;
        auto ease = compositor.CreateCubicBezierEasingFunction({0.4f, 0}, {1, 1});
        auto opacity = compositor.CreateScalarKeyFrameAnimation();
        opacity.Duration(std::chrono::milliseconds(duration));
        // StartingValue preserves continuity when release interrupts opening.
        opacity.InsertExpressionKeyFrame(0, L"this.StartingValue");
        opacity.InsertKeyFrame(1, 0, ease);
        animationRoot.StartAnimation(L"Opacity", opacity);
        auto scale = compositor.CreateVector3KeyFrameAnimation();
        scale.Duration(std::chrono::milliseconds(duration));
        scale.InsertExpressionKeyFrame(0, L"this.StartingValue");
        scale.InsertKeyFrame(1, {0.96f, 0.96f, 1}, ease);
        animationRoot.StartAnimation(L"Scale", scale);
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
        L"; animation=settle-160ms/exit-120ms; center=quiet; perimeter=neutral; anchor=" + std::to_wstring(impl_->anchorX) +
        L"," + std::to_wstring(impl_->anchorY) +
        L"; DPI=" + std::to_wstring(impl_->dpi) + L"; surface draws=" + std::to_wstring(impl_->surfaceDraws) +
        L"; device=" + (impl_->softwareDevice ? L"WARP" : L"hardware") +
        (impl_->lastError.empty() ? L"" : L"; last error=" + impl_->lastError);
}
} // namespace reel
