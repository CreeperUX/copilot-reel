#pragma once
#include "app_types.h"
#include <memory>

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
    // Compatibility no-op: all icons now use the shared vector symbol set.
    void SetIcons(const std::vector<HICON>& icons);
    void SetAwaitCenter(bool awaitCenter);
    void SetSelection(int index);
    void Show();
    void Hide();
    bool UsesBackdrop() const;
    std::wstring Diagnostics() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
