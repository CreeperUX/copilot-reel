#include "core.h"
#include <iostream>
#include <limits>
#include <cstdlib>

void Check(bool condition, const char* label) {
    if (!condition) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
}
int main() {
    using namespace reel;
    Check(HitTest(0,0,6)==-1, "center cancels");
    Check(HitTest(44,0,6)==-1, "dead zone boundary cancels");
    for (unsigned count=1; count<=8; ++count) {
        for (unsigned i=0; i<count; ++i) {
            const double angle=-kPi/2 + i*2*kPi/count;
            Check(HitTest(112*std::cos(angle),112*std::sin(angle),count)==static_cast<int>(i), "sector centers match shared layout");
            Check(HitTest(10000*std::cos(angle),10000*std::sin(angle),count)==static_cast<int>(i), "outside ring preserves direction");
        }
    }
    Check(HitTest(10,100,0)==-1, "empty targets");
    Check(HitTest(std::numeric_limits<double>::quiet_NaN(),100,6)==-1, "invalid pointer rejected");
    GestureState g;
    Check(!g.Release(), "orphan stop cannot launch");
    Check(g.Start(), "start arms");
    Check(!g.Start(), "duplicate start is idempotent");
    Check(!g.Release(), "center release cancels");
    g.Start(); g.Select(2,6);
    Check(g.Release()==2, "valid release commits");
    Check(!g.Release(), "duplicate stop never commits twice");
    g.Start(); const auto old=g.Generation(); g.Select(2,6); g.Cancel();
    Check(g.Generation()!=old && !g.Active() && !g.Release(), "cancel invalidates pending work");
    g.Start(); g.Select(7,6);
    Check(!g.Release(), "stale config index cannot launch");
    Check(ParseUri(L"copilot-reel://?state=Down")==Command::HoldStart, "protocol down");
    Check(ParseUri(L"copilot-reel://?state=Up")==Command::HoldStop, "protocol up");
    Check(ParseUri(L"copilot-reel://?state=Tap")==Command::Tap, "protocol tap");
    Check(ParseUri(L"copilot-reel://?state=Up&target=evil.exe")==Command::Invalid, "URI cannot inject target");
    Check(ParseUri(L"https://example.com/?state=Down")==Command::Invalid, "foreign URI rejected");
    std::cout << "Core tests passed: 1-8 sector geometry, center/outer regions, gesture lifetime, exactly-once release, protocol validation.\n";
}
