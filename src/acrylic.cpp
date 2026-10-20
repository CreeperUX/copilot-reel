#include <unknwn.h>
#include <d2d1_1.h>
#include "acrylic.h"
#include "vi_tokens.h"

#include <d2d1effects.h>
#include <windows.graphics.effects.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <cmath>
#include <utility>

namespace reel {
namespace {
namespace wc = winrt::Windows::UI::Composition;
namespace wf = winrt::Windows::Foundation;
namespace we = winrt::Windows::Graphics::Effects;
namespace ae = ABI::Windows::Graphics::Effects;

// Describes one public Direct2D effect to Windows Composition. Properties are
// indexed by their D2D property index; none is animatable.
struct Effect : winrt::implements<Effect, we::IGraphicsEffect, we::IGraphicsEffectSource, ae::IGraphicsEffectD2D1Interop> {
    Effect(GUID id, std::vector<wf::IPropertyValue> properties, std::vector<we::IGraphicsEffectSource> sources)
        : id_(id), properties_(std::move(properties)), sources_(std::move(sources)) {}
    winrt::hstring Name() const { return name_; }
    void Name(winrt::hstring const& name) { name_ = name; }
    HRESULT __stdcall GetEffectId(GUID* id) noexcept final {
        if (!id) return E_POINTER;
        *id = id_;
        return S_OK;
    }
    HRESULT __stdcall GetNamedPropertyMapping(LPCWSTR, UINT*, ae::GRAPHICS_EFFECT_PROPERTY_MAPPING*) noexcept final {
        return E_INVALIDARG;
    }
    HRESULT __stdcall GetPropertyCount(UINT* count) noexcept final {
        if (!count) return E_POINTER;
        *count = static_cast<UINT>(properties_.size());
        return S_OK;
    }
    HRESULT __stdcall GetProperty(UINT index, ABI::Windows::Foundation::IPropertyValue** value) noexcept final {
        if (!value) return E_POINTER;
        *value = nullptr;
        if (index >= properties_.size()) return E_INVALIDARG;
        auto copy = properties_[index];
        *value = reinterpret_cast<ABI::Windows::Foundation::IPropertyValue*>(winrt::detach_abi(copy));
        return S_OK;
    }
    HRESULT __stdcall GetSource(UINT index, ae::IGraphicsEffectSource** source) noexcept final {
        if (!source) return E_POINTER;
        *source = nullptr;
        if (index >= sources_.size()) return E_INVALIDARG;
        try {
            sources_[index].as<ae::IGraphicsEffectSource>().copy_to(source);
            return S_OK;
        } catch (...) { return winrt::to_hresult(); }
    }
    HRESULT __stdcall GetSourceCount(UINT* count) noexcept final {
        if (!count) return E_POINTER;
        *count = static_cast<UINT>(sources_.size());
        return S_OK;
    }
private:
    GUID id_;
    winrt::hstring name_;
    std::vector<wf::IPropertyValue> properties_;
    std::vector<we::IGraphicsEffectSource> sources_;
};

wf::IPropertyValue Value(UINT32 value) { return wf::PropertyValue::CreateUInt32(value).as<wf::IPropertyValue>(); }
we::IGraphicsEffectSource Make(GUID id, std::vector<wf::IPropertyValue> properties,
                               std::vector<we::IGraphicsEffectSource> sources = {}) {
    return winrt::make<Effect>(id, std::move(properties), std::move(sources));
}
we::IGraphicsEffectSource Flood(const vi::Color& color, float alpha) {
    const float rgba[]{color.r, color.g, color.b, alpha};
    return Make(CLSID_D2D1Flood, {wf::PropertyValue::CreateSingleArray(rgba).as<wf::IPropertyValue>()});
}
// Composition swaps the Color and Luminosity blend modes relative to their names
// (microsoft-ui-xaml AcrylicBrush). These values reproduce WinUI's acrylic graph.
we::IGraphicsEffectSource Blend(D2D1_BLEND_MODE mode, we::IGraphicsEffectSource background,
                                we::IGraphicsEffectSource foreground) {
    return Make(CLSID_D2D1Blend, {Value(mode)}, {std::move(background), std::move(foreground)});
}
} // namespace

wc::CompositionEffectBrush CreateAcrylicBrush(wc::Compositor const& compositor, bool dark,
                                              wc::CompositionBrush const& noise) {
    const auto& tint = dark ? vi::kAcrylicTintDark : vi::kAcrylicTintLight;
    const float tintOpacity = dark ? vi::kAcrylicTintOpacityDark : vi::kAcrylicTintOpacityLight;
    const float luminosity = dark ? vi::kAcrylicLuminosityDark : vi::kAcrylicLuminosityLight;
    we::IGraphicsEffectSource backdrop = wc::CompositionEffectSourceParameter(L"Backdrop");
    we::IGraphicsEffectSource noiseSource = wc::CompositionEffectSourceParameter(L"Noise");
    // Luminosity of the luminosity color over the backdrop's hue, then the tint's color.
    auto luminosityBlend = Blend(D2D1_BLEND_MODE_COLOR, backdrop, Flood(tint, luminosity));
    auto tintBlend = Blend(D2D1_BLEND_MODE_LUMINOSITY, luminosityBlend, Flood(tint, tintOpacity));
    auto tiledNoise = Make(CLSID_D2D1Border,
        {Value(D2D1_BORDER_EDGE_MODE_WRAP), Value(D2D1_BORDER_EDGE_MODE_WRAP)}, {noiseSource});
    auto graph = Make(CLSID_D2D1Composite, {Value(D2D1_COMPOSITE_MODE_SOURCE_OVER)}, {tintBlend, tiledNoise});
    auto brush = compositor.CreateEffectFactory(graph.as<we::IGraphicsEffect>()).CreateBrush();
    brush.SetSourceParameter(L"Backdrop", compositor.CreateHostBackdropBrush());
    brush.SetSourceParameter(L"Noise", noise);
    return brush;
}

std::vector<BYTE> AcrylicNoisePixels() {
    constexpr unsigned size = vi::kAcrylicNoiseSize;
    std::vector<BYTE> pixels(static_cast<size_t>(size) * size * 4);
    const unsigned alpha = static_cast<unsigned>(std::lround(vi::kAcrylicNoiseOpacity * 255));
    // Deterministic uniform grey noise (LCG); premultiplied by its low alpha.
    unsigned state = 0x2545F491u;
    for (size_t i = 0; i < pixels.size(); i += 4) {
        state = state * 1664525u + 1013904223u;
        const unsigned grey = (state >> 24) * alpha / 255;
        pixels[i] = pixels[i + 1] = pixels[i + 2] = static_cast<BYTE>(grey);
        pixels[i + 3] = static_cast<BYTE>(alpha);
    }
    return pixels;
}
} // namespace reel
