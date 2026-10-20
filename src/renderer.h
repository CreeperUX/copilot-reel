#pragma once
#include "app_types.h"
#include "localization.h"
#include <d2d1.h>
#include <memory>
#include <utility>

namespace reel {
// How a page replaces the previous one. DrillIn/DrillOut keep the plate still
// and move only the orbit contents; Swap is a same-level page change.
enum class PageTransition { None, DrillIn, DrillOut, Swap };
// Commit keeps the chosen item for one beat while the rest of the reel leaves.
enum class HideStyle { Cancel, Commit };
inline const wchar_t* TransitionName(PageTransition transition) {
    switch (transition) {
    case PageTransition::DrillIn: return L"drill-in";
    case PageTransition::DrillOut: return L"drill-out";
    case PageTransition::Swap: return L"swap";
    default: return L"none";
    }
}

struct PageItem {
    AppTarget visual;
    bool enabled = true;   // disabled items keep their slot, draw at reduced opacity and cannot be chosen
};

struct PageModel {
    std::vector<PageItem> items;
    std::wstring title;          // diagnostics only; never drawn
    bool isSubmenu = false;      // idle center shows the back chevron
    bool languageControl = false;// slot 0 is the EN/ZH tile with chevrons
    int preferredIndex = -1;
    int selection = -1;
    bool awaitCenter = false;
};

// All calls and lifetime belong to the HWND's UI thread.
class OrbitRenderer {
public:
    explicit OrbitRenderer(HWND window);
    ~OrbitRenderer();
    OrbitRenderer(const OrbitRenderer&) = delete;
    OrbitRenderer& operator=(const OrbitRenderer&) = delete;
    bool Initialize(std::wstring& error);
    void Resize(UINT dpi);
    // Idempotent: unchanged appearance does not rebuild the material or redraw.
    void SetAppearance(const Appearance& appearance);
    // Rebuilds text formats with the language's locale and CJK fallback.
    void SetLanguage(Language language);
    // Replaces the page. originIndex is the slot in the previous page whose
    // tile triggered a DrillIn (its icon travels to the center), or -1.
    void SetPage(const PageModel& page, PageTransition transition = PageTransition::None, int originIndex = -1);
    // Local canvas DIP hit rectangles beside the language tile at the top orbit.
    std::pair<D2D1_RECT_F, D2D1_RECT_F> LanguageArrowBounds() const;
    void SetAwaitCenter(bool awaitCenter);
    void SetSelection(int index);
    void Show();
    // Starts a compositor-only exit and returns the milliseconds until Hide may detach.
    unsigned BeginHide(HideStyle style = HideStyle::Cancel);
    void Hide();
    bool UsesBackdrop() const;
    std::wstring Diagnostics() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
