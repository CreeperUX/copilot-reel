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
    for(const WorkArea work : {WorkArea{0,0,1920,1040},WorkArea{-2560,-200,0,1240}}) {
        for(int size : {384,576,768}) {
            const int xs[]={work.left,work.right-1,(work.left+work.right)/2};
            const int ys[]={work.top,work.bottom-1,(work.top+work.bottom)/2};
            for(int x:xs) for(int y:ys) {
                const auto placement=PlaceWheel(x,y,work,size,44.*size/384.);
                Check(placement.fits,"normal footprint fits");
                Check(placement.left>=work.left && placement.top>=work.top && placement.left+size<=work.right && placement.top+size<=work.bottom,"full circle fits monitor work area");
                if(x==work.left || x==work.right-1 || y==work.top || y==work.bottom-1)
                    Check(placement.awaitingCenter,"edge opening requires center entry");
            }
        }
    }
    Check(!PlaceWheel(0,0,{0,0,300,300},384,44).fits,"undersized display never clips silently");
    const auto interior=PlaceWheel(600,400,{0,0,1920,1040},384,44);
    Check(interior.centerX==600 && interior.centerY==400 && !interior.awaitingCenter,"interior stays pointer-centered");
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
    g.Start(true);
    Check(!g.Select(1,6) && !g.ObservePointer(100,-100,6) && g.Selected()==-1,"crossing a displaced item stays unselected");
    Check(!g.Release(),"release before center entry cancels");
    g.Start(true);
    Check(g.ObservePointer(0,0,6) && !g.AwaitingCenter() && g.Selected()==-1,"center entry arms without selecting");
    Check(g.ObservePointer(0,-112,6) && g.Release()==0,"armed selection uses visual center");
    g.Start(true);g.ObservePointer(0,0,6);g.ObservePointer(112,0,6);g.ObservePointer(0,0,6);
    Check(!g.Release(),"returning to center cancels after arming");
    Check(ParseUri(L"copilot-reel://?state=Down")==Command::HoldStart, "protocol down");
    Check(ParseUri(L"copilot-reel://?state=Up")==Command::HoldStop, "protocol up");
    Check(ParseUri(L"copilot-reel://?state=Tap")==Command::Tap, "protocol tap");
    Check(ParseUri(L"copilot-reel://?state=Up&target=evil.exe")==Command::Invalid, "URI cannot inject target");
    Check(ParseUri(L"https://example.com/?state=Down")==Command::Invalid, "foreign URI rejected");
    std::cout << "Core tests passed: 1-8 sector geometry, center/outer regions, gesture lifetime, exactly-once release, protocol validation.\n";
}
