#pragma once
#include <cmath>
#include <optional>
#include <string_view>

namespace reel {
constexpr double kPi = 3.14159265358979323846;

inline int HitTest(double dx, double dy, unsigned count, double deadZone = 44.0) {
    if (count == 0 || count > 8 || !std::isfinite(dx) || !std::isfinite(dy) ||
        dx * dx + dy * dy <= deadZone * deadZone) return -1;
    double angle = std::atan2(dy, dx) + kPi / 2;
    if (angle < 0) angle += 2 * kPi;
    const double step = 2 * kPi / count;
    return static_cast<int>(std::floor((angle + step / 2) / step)) % count;
}

class GestureState {
public:
    bool Start() {
        if (active_) return false;
        active_ = true; selected_ = -1; ++generation_; return true;
    }
    bool Select(int index, unsigned count) {
        if (!active_) return false;
        const int valid = index >= 0 && static_cast<unsigned>(index) < count ? index : -1;
        if (valid == selected_) return false;
        selected_ = valid; return true;
    }
    std::optional<int> Release() {
        if (!active_) return {};
        const int result = selected_;
        Cancel();
        return result < 0 ? std::optional<int>{} : std::optional<int>{result};
    }
    void Cancel() { active_ = false; selected_ = -1; ++generation_; }
    bool Active() const { return active_; }
    int Selected() const { return selected_; }
    unsigned Generation() const { return generation_; }
private:
    bool active_ = false;
    int selected_ = -1;
    unsigned generation_ = 0;
};

enum class Command : unsigned { Background, Settings, Preview, HoldStart, HoldStop, Cancel, Quit, Tap, Diagnostics, SmokeTest, Invalid };

inline Command ParseUri(std::wstring_view uri) {
    // This private activation protocol intentionally accepts no launch targets.
    if (uri == L"copilot-reel://?state=Down") return Command::HoldStart;
    if (uri == L"copilot-reel://?state=Up") return Command::HoldStop;
    if (uri == L"copilot-reel://?state=Tap") return Command::Tap;
    return Command::Invalid;
}
}
