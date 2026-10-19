#include "appearance.h"
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.Power.h>
#include <atomic>
#include <algorithm>

namespace reel {
namespace {
Rgba Color(COLORREF c) { return {GetRValue(c)/255.f,GetGValue(c)/255.f,GetBValue(c)/255.f,1}; }
Rgba Color(winrt::Windows::UI::Color c) { return {c.R/255.f,c.G/255.f,c.B/255.f,1}; }
Rgba Mix(Rgba a,Rgba b,float amount) {
    return {a.r+(b.r-a.r)*amount,a.g+(b.g-a.g)*amount,a.b+(b.b-a.b)*amount,1};
}
struct CallbackState { std::atomic<HWND> window{nullptr}; };
}
struct AppearanceSource::Impl {
    winrt::Windows::UI::ViewManagement::UISettings settings{nullptr};
    winrt::event_token colors{}, effects{}, animations{}, power{};
    bool subscribedColors=false, subscribedEffects=false, subscribedAnimations=false, subscribedPower=false;
    std::shared_ptr<CallbackState> callback = std::make_shared<CallbackState>();
    explicit Impl(HWND hwnd) {
        callback->window.store(hwnd);
        auto notify=[state=callback](auto const&,auto const&) {
            if (HWND target=state->window.load()) PostMessageW(target,kAppearanceMessage,0,0);
        };
        try {
            settings=winrt::Windows::UI::ViewManagement::UISettings();
            colors=settings.ColorValuesChanged(notify); subscribedColors=true;
            effects=settings.AdvancedEffectsEnabledChanged(notify); subscribedEffects=true;
            animations=settings.AnimationsEnabledChanged(notify); subscribedAnimations=true;
            power=winrt::Windows::System::Power::PowerManager::EnergySaverStatusChanged(notify); subscribedPower=true;
        } catch (...) { /* Appearance::Read still provides a conservative solid fallback. */ }
    }
    ~Impl() {
        callback->window.store(nullptr);
        try { if(subscribedColors) settings.ColorValuesChanged(colors); } catch(...) {}
        try { if(subscribedEffects) settings.AdvancedEffectsEnabledChanged(effects); } catch(...) {}
        try { if(subscribedAnimations) settings.AnimationsEnabledChanged(animations); } catch(...) {}
        try { if(subscribedPower) winrt::Windows::System::Power::PowerManager::EnergySaverStatusChanged(power); } catch(...) {}
    }
};
AppearanceSource::AppearanceSource(HWND hwnd):impl_(std::make_unique<Impl>(hwnd)) {}
AppearanceSource::~AppearanceSource()=default;
Appearance AppearanceSource::Read() {
    Appearance out;
    HIGHCONTRASTW hc{sizeof(hc)};
    out.highContrast=SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(hc),&hc,0) && (hc.dwFlags&HCF_HIGHCONTRASTON);
    BOOL enabled=TRUE;
    if(SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&enabled,0)) out.animations=enabled!=FALSE;
    try {
        if(impl_->settings) {
            using winrt::Windows::UI::ViewManagement::UIColorType;
            auto fg=impl_->settings.GetColorValue(UIColorType::Foreground);
            out.dark=(5*fg.G+2*fg.R+fg.B)>8*128;
            out.transparency=impl_->settings.AdvancedEffectsEnabled();
            out.animations=impl_->settings.AnimationsEnabled();
            out.accent=Color(impl_->settings.GetColorValue(UIColorType::Accent));
        }
        if(winrt::Windows::System::Power::PowerManager::EnergySaverStatus()==winrt::Windows::System::Power::EnergySaverStatus::On)
            out.transparency=false;
    } catch(...) { out.transparency=false; }
    if(out.dark) {
        out.surface={.161f,.176f,.20f,1}; out.foreground={.953f,.961f,.973f,1};
        out.secondary={.745f,.773f,.808f,1}; out.outline={.329f,.361f,.404f,1};
    }
    out.selection=Mix(out.surface,out.accent,out.dark?.25f:.13f);
    out.selectionText=out.foreground;
    if(out.highContrast) {
        out.transparency=false;
        out.surface=Color(GetSysColor(COLOR_WINDOW));
        out.foreground=Color(GetSysColor(COLOR_WINDOWTEXT));
        out.secondary=out.foreground; out.outline=out.foreground;
        out.accent=Color(GetSysColor(COLOR_HIGHLIGHT));
        out.selection=out.accent;
        out.selectionText=Color(GetSysColor(COLOR_HIGHLIGHTTEXT));
    }
    return out;
}
}
