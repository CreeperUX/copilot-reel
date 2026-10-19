#pragma once
#include "app_types.h"
#include <memory>

namespace reel {
class AppearanceSource {
public:
    explicit AppearanceSource(HWND target);
    ~AppearanceSource();
    AppearanceSource(const AppearanceSource&) = delete;
    AppearanceSource& operator=(const AppearanceSource&) = delete;
    Appearance Read();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
