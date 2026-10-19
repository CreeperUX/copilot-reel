#pragma once
#include "app_types.h"
#include <d2d1.h>
#include <memory>
#include <utility>

namespace reel {
// All calls and lifetime belong to the HWND's UI thread.
class OrbitRenderer {
public:
    explicit OrbitRenderer(HWND window);
    ~OrbitRenderer();
    OrbitRenderer(const OrbitRenderer&) = delete;
    OrbitRenderer& operator=(const OrbitRenderer&) = delete;
    bool Initialize(std::wstring& error);
    void Resize(UINT dpi);
    void SetAppearance(const Appearance& appearance);
    void SetItems(const std::vector<AppTarget>& items);
    // Compatibility no-op: recognized applications use packaged official artwork.
    void SetIcons(const std::vector<HICON>& icons);
    void SetMenuContext(const std::wstring& title, bool isSubmenu, bool languageControl);
    void SetPreferredIndex(int index);
    // Original pointer position relative to the final wheel center, in DIPs.
    void SetAnchorOffset(float x, float y);
    // Local canvas DIP rectangles shared with the controller's hit testing.
    std::pair<D2D1_RECT_F, D2D1_RECT_F> LanguageArrowBounds() const;
    void SetAwaitCenter(bool awaitCenter);
    void SetSelection(int index);
    void Show();
    // Starts a compositor-only exit and returns its duration for a one-shot timer.
    unsigned BeginHide();
    void Hide();
    bool UsesBackdrop() const;
    std::wstring Diagnostics() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
