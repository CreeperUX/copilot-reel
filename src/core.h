#pragma once
#include <cmath>
#include <algorithm>
#include <optional>
#include <string_view>
#include <cstdint>

namespace reel {
constexpr double kPi = 3.14159265358979323846;

struct WorkArea { int left, top, right, bottom; };
struct WheelPlacement {
    int left = 0, top = 0, size = 0;
    double centerX = 0, centerY = 0;
    bool fits = false, awaitingCenter = false;
};

// Use the complete drawing footprint, including shadow padding. All positions
// are physical screen pixels; negative coordinates support secondary monitors.
inline WheelPlacement PlaceWheel(int pointerX, int pointerY, WorkArea work,
                                int size, double deadZonePixels) {
    WheelPlacement result;
    if(size <= 0 || work.right-work.left < size || work.bottom-work.top < size) return result;
    result.left=std::clamp(pointerX-size/2,work.left,work.right-size);
    result.top=std::clamp(pointerY-size/2,work.top,work.bottom-size);
    result.size=size;
    result.centerX=result.left+size/2.0;
    result.centerY=result.top+size/2.0;
    result.fits=true;
    const double dx=pointerX-result.centerX,dy=pointerY-result.centerY;
    result.awaitingCenter=dx*dx+dy*dy>deadZonePixels*deadZonePixels;
    return result;
}

// Choose an inward-facing spoke near the pointer. Keep the complete circle on
// screen; at corners this necessarily lengthens the requested radius tether.
inline WheelPlacement PlaceOrbit(int x, int y, WorkArea work, int size, double deadZone) {
    const double radius=size*168.0/384.0;
    constexpr double directions[][2]={{.70710678,-.70710678},{-.70710678,-.70710678},
        {.70710678,.70710678},{-.70710678,.70710678},{1,0},{-1,0},{0,-1},{0,1}};
    WheelPlacement best;
    double score=1e30;
    for(const auto& direction:directions) {
        const int desiredX=x+static_cast<int>(std::round(radius*direction[0]));
        const int desiredY=y+static_cast<int>(std::round(radius*direction[1]));
        auto candidate=PlaceWheel(desiredX,desiredY,work,size,deadZone);
        if(!candidate.fits) return candidate;
        const double dx=candidate.centerX-desiredX,dy=candidate.centerY-desiredY;
        const double deviation=dx*dx+dy*dy;
        if(deviation<score) {best=candidate;score=deviation;}
        if(deviation<1) break;
    }
    const double dx=x-best.centerX,dy=y-best.centerY;
    best.awaitingCenter=dx*dx+dy*dy>deadZone*deadZone;
    return best;
}

// URI activations can arrive out of process-creation order. An Up tombstone
// rejects a delayed Down from the same older physical gesture.
class HoldOrdering {
public:
    bool Start(uint64_t stamp) {
        if(stamp<=lastStop_ || stamp<=lastStart_) return false;
        lastStart_=stamp;return true;
    }
    bool Stop(uint64_t stamp) {
        lastStop_=std::max(lastStop_,stamp);
        return stamp>=lastStart_;
    }
private:
    uint64_t lastStart_=0,lastStop_=0;
};

inline int HitTest(double dx, double dy, unsigned count, double deadZone = 44.0) {
    if (count == 0 || count > 12 || !std::isfinite(dx) || !std::isfinite(dy) ||
        dx * dx + dy * dy <= deadZone * deadZone) return -1;
    double angle = std::atan2(dy, dx) + kPi / 2;
    if (angle < 0) angle += 2 * kPi;
    const double step = 2 * kPi / count;
    return static_cast<int>(std::floor((angle + step / 2) / step)) % count;
}

class GestureState {
public:
    bool Start(bool awaitCenter = false) {
        if (active_) return false;
        active_ = true; awaitingCenter_ = awaitCenter; selected_ = -1; ++generation_; return true;
    }
    bool Select(int index, unsigned count) {
        if (!active_ || awaitingCenter_) return false;
        const int valid = index >= 0 && static_cast<unsigned>(index) < count ? index : -1;
        if (valid == selected_) return false;
        selected_ = valid; return true;
    }
    bool ObservePointer(double dx,double dy,unsigned count,double deadZone=44.0) {
        if(!active_ || !std::isfinite(dx) || !std::isfinite(dy)) return false;
        if(awaitingCenter_) {
            if(dx*dx+dy*dy<=deadZone*deadZone) {awaitingCenter_=false;return true;}
            return false;
        }
        return Select(HitTest(dx,dy,count,deadZone),count);
    }
    std::optional<int> Release() {
        if (!active_) return {};
        const int result = selected_;
        Cancel();
        return result < 0 ? std::optional<int>{} : std::optional<int>{result};
    }
    void Cancel() { active_ = false; awaitingCenter_ = false; selected_ = -1; ++generation_; }
    bool Active() const { return active_; }
    bool AwaitingCenter() const { return awaitingCenter_; }
    int Selected() const { return selected_; }
    unsigned Generation() const { return generation_; }
private:
    bool active_ = false;
    bool awaitingCenter_ = false;
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
