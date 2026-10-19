#include "renderer.h"
#include "iconography.h"
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
    unsigned surfaceDraws = 0;
    int selection = -1;
    Appearance appearance;
    std::vector<AppTarget> items;
    std::wstring lastError;
    std::wstring backdropStatus = L"solid (transparency disabled)";
    winrt::Windows::System::DispatcherQueueController queueController{nullptr};
    wc::Compositor compositor{nullptr};
    winrt::Windows::UI::Composition::Desktop::DesktopWindowTarget target{nullptr};
    wc::ContainerVisual pixelRoot{nullptr};
    wc::ContainerVisual dpiRoot{nullptr};
    wc::ContainerVisual animationRoot{nullptr};
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
            animationRoot.StopAnimation(L"Opacity");
            animationRoot.StopAnimation(L"Scale");
            animationRoot.Opacity(1.0f);
            animationRoot.Scale({1, 1, 1});
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
                // Every target uses the same vector tile. No shell bitmaps or
                // font-dependent initials can leak a different visual language.
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
                DrawAppSymbol(context.get(), items[i], D2D1::RectF(x - 14, y - 14, x + 14, y + 14),
                    brush.get(), appearance.highContrast ? 2.0f : 1.75f);
                // Full names live in the centre; tiles remain icon-first and do
                // not shift or collide when the user configures eight targets.
                if (selected) {
                    brush->SetColor(Color(appearance.highContrast ? appearance.selectionText : appearance.accent));
                    context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y + 26), 2.0f, 2.0f), brush.get());
                }
            }
            const bool selected = !awaitCenter && selection >= 0 && static_cast<size_t>(selection) < items.size();
            brush->SetColor(Color(appearance.foreground));
            Text(context.get(), awaitCenter ? Tr(L"Move", L"移入") :
                selected ? items[selection].name : Tr(L"Cancel",L"松开取消"),
                D2D1::RectF(kCenterDip - 40, kCenterDip - 17, kCenterDip + 40, kCenterDip + 5),
                centerFormat.get(), brush.get());
            brush->SetColor(Color(appearance.secondary));
            Text(context.get(), awaitCenter ? Tr(L"to center", L"中心后选择") :
                selected ? Tr(L"Release",L"松开打开") : Tr(L"Choose app",L"移向应用"),
                D2D1::RectF(kCenterDip - 40, kCenterDip + 7, kCenterDip + 40, kCenterDip + 25),
                hintFormat.get(), brush.get());
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

    void Show() {
        if (FAILED(d3d->GetDeviceRemovedReason())) Draw();
        target.Root(pixelRoot);
        visible = true;
        animationRoot.StopAnimation(L"Opacity");
        animationRoot.StopAnimation(L"Scale");
        animationRoot.Opacity(1.0f);
        animationRoot.Scale({1, 1, 1});
        if (appearance.animations && !appearance.highContrast) {
            auto ease = compositor.CreateCubicBezierEasingFunction({0.16f, 1.0f}, {0.3f, 1.0f});
            auto opacity = compositor.CreateScalarKeyFrameAnimation();
            opacity.Duration(std::chrono::milliseconds(100));
            opacity.InsertKeyFrame(0.0f, 0.0f);
            opacity.InsertKeyFrame(1.0f, 1.0f, ease);
            auto scale = compositor.CreateVector3KeyFrameAnimation();
            scale.Duration(std::chrono::milliseconds(100));
            scale.InsertKeyFrame(0.0f, {0.98f, 0.98f, 1});
            scale.InsertKeyFrame(1.0f, {1, 1, 1}, ease);
            animationRoot.StartAnimation(L"Opacity", opacity);
            animationRoot.StartAnimation(L"Scale", scale);
        }
    }

    void Hide() {
        visible = false;
        animationRoot.StopAnimation(L"Opacity");
        animationRoot.StopAnimation(L"Scale");
        target.Root(nullptr);
    }
};

OrbitRenderer::OrbitRenderer(HWND window) : impl_(std::make_unique<Impl>(window)) {}
OrbitRenderer::~OrbitRenderer() {
    if (!impl_) return;
    impl_->Safe([&] {
        if (impl_->target) impl_->target.Root(nullptr);
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
    // Retained for existing callers. The vector symbol set is authoritative.
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
void OrbitRenderer::Hide() {
    if (impl_->ready) impl_->Safe([&] { impl_->Hide(); });
}
bool OrbitRenderer::UsesBackdrop() const { return impl_->backdrop; }
std::wstring OrbitRenderer::Diagnostics() const {
    return L"Windows Composition + Direct2D/DirectWrite; icons=unified-vector; " + impl_->backdropStatus +
        L"; center gate=" + (impl_->awaitCenter ? L"waiting" : L"armed") +
        L"; DPI=" + std::to_wstring(impl_->dpi) + L"; surface draws=" + std::to_wstring(impl_->surfaceDraws) +
        L"; device=" + (impl_->softwareDevice ? L"WARP" : L"hardware") +
        (impl_->lastError.empty() ? L"" : L"; last error=" + impl_->lastError);
}
} // namespace reel
