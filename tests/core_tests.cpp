#include "core.h"
#include "radial_menu.h"
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
    for (unsigned count=1; count<=12; ++count) {
        for (unsigned i=0; i<count; ++i) {
            const double angle=-kPi/2 + i*2*kPi/count;
            Check(HitTest(112*std::cos(angle),112*std::sin(angle),count)==static_cast<int>(i), "sector centers match shared layout");
            Check(HitTest(10000*std::cos(angle),10000*std::sin(angle),count)==static_cast<int>(i), "outside ring preserves direction");
        }
    }
    Check(HitTest(10,100,0)==-1, "empty targets");
    Check(HitTest(10,100,13)==-1, "unsupported sector count rejected");
    Check(HitTest(std::numeric_limits<double>::quiet_NaN(),100,6)==-1, "invalid pointer rejected");
    Check(HitTest(100,std::numeric_limits<double>::infinity(),12)==-1, "infinite pointer rejected");
    for(unsigned count=2;count<=12;++count) {
        const double step=2*kPi/count;
        for(unsigned i=0;i<count;++i) {
            const double boundary=-kPi/2+(i+.5)*step;
            Check(HitTest(112*std::cos(boundary-1e-7),112*std::sin(boundary-1e-7),count)==static_cast<int>(i),"sector boundary preserves earlier side");
            Check(HitTest(112*std::cos(boundary+1e-7),112*std::sin(boundary+1e-7),count)==static_cast<int>((i+1)%count),"sector boundary advances to next side");
        }
    }
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
    for(const WorkArea work : {WorkArea{0,0,1920,1040},WorkArea{-3840,-2160,0,0},WorkArea{1920,-1080,5760,2160}}) {
        for(int dpi : {96,120,144,168,192,288,384}) {
            const int size=384*dpi/96;
            const double deadZone=44.0*dpi/96;
            const int xs[]={work.left,work.left+1,(work.left+work.right)/2,work.right-2,work.right-1};
            const int ys[]={work.top,work.top+1,(work.top+work.bottom)/2,work.bottom-2,work.bottom-1};
            for(int x:xs)for(int y:ys) {
                const auto placement=PlaceOrbit(x,y,work,size,deadZone);
                const bool shouldFit=work.right-work.left>=size && work.bottom-work.top>=size;
                Check(placement.fits==shouldFit,"orbit fit respects the complete physical footprint at every DPI");
                if(!shouldFit)continue;
                Check(placement.size==size && placement.left>=work.left && placement.top>=work.top &&
                    placement.left+size<=work.right && placement.top+size<=work.bottom,
                    "orbit and shadow stay inside all monitor edges and corners");
                Check(placement.centerX==placement.left+size/2.0 && placement.centerY==placement.top+size/2.0,
                    "orbit hit-test origin equals its rendered center");
                const int expectedLeft=std::clamp(x-size/2,work.left,work.right-size);
                const int expectedTop=std::clamp(y-size/2,work.top,work.bottom-size);
                Check(placement.left==expectedLeft && placement.top==expectedTop,
                    "placement uses only the minimum translation required by the work area");
                const double dx=x-placement.centerX,dy=y-placement.centerY;
                Check(placement.awaitingCenter==(dx*dx+dy*dy>deadZone*deadZone),
                    "orbit arms only when the original pointer is inside the displayed cancel zone");
                constexpr int offsetX=-5000,offsetY=777;
                const auto translated=PlaceOrbit(x+offsetX,y+offsetY,
                    {work.left+offsetX,work.top+offsetY,work.right+offsetX,work.bottom+offsetY},size,deadZone);
                Check(translated.left==placement.left+offsetX && translated.top==placement.top+offsetY &&
                    translated.awaitingCenter==placement.awaitingCenter,
                    "orbit placement is invariant under monitor-origin translation");
            }
        }
    }
    for(int dpi : {96,120,144,192,288,384}) {
        const int size=384*dpi/96;
        const auto placement=PlaceOrbit(0,0,{-5000,-5000,5000,5000},size,44.0*dpi/96);
        Check(placement.centerX==0 && placement.centerY==0,
            "unconstrained wheel opens directly on the pointer at every DPI");
        Check(!placement.awaitingCenter,"an unconstrained hold is immediately ready for directional selection");
    }
    Check(!PlaceOrbit(0,0,{0,0,300,300},384,44).fits,"orbit rejects a screen smaller than its footprint");
    Check(!PlaceOrbit(0,0,{0,0,1920,1080},0,44).fits,"orbit rejects an empty canvas");
    const auto corner=PlaceOrbit(-3840,-2160,{-3840,-2160,0,0},768,88);
    Check(corner.left==-3840 && corner.top==-2160 && corner.awaitingCenter,
        "a corner shifts the full canvas minimally and guards selection until center entry");
    const auto smallShift=PlaceOrbit(180,400,{0,0,1920,1080},384,44);
    Check(smallShift.centerX==192 && smallShift.centerY==400 && !smallShift.awaitingCenter,
        "small boundary correction within the cancel zone does not add an unnecessary gate");
    const auto oddCanvas=PlaceOrbit(600,400,{0,0,1920,1080},383,44);
    Check(std::abs(oddCanvas.centerX-600)<=.5 && std::abs(oddCanvas.centerY-400)<=.5 && !oddCanvas.awaitingCenter,
        "odd pixel dimensions remain pointer-centered within half a pixel");
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
    HoldOrdering ordering;
    Check(ordering.Start(100),"first Down is accepted");
    Check(!ordering.Start(100) && !ordering.Start(99),"duplicate and stale Down are rejected");
    Check(!ordering.Stop(99),"older Up cannot close a newer hold");
    Check(ordering.Stop(101),"matching later Up closes the hold");
    Check(!ordering.Start(100) && !ordering.Start(101),"Up tombstone rejects an equal or earlier Down");
    Check(ordering.Start(102) && !ordering.Stop(101) && ordering.Stop(103),"next hold is independent of stale prior Up");
    HoldOrdering cold;
    Check(cold.Stop(200),"orphan cold Up records a tombstone");
    Check(!cold.Start(199) && !cold.Start(200),"cold Up arriving first suppresses a delayed older Down");
    Check(cold.Start(201) && cold.Stop(202),"later deliberate hold remains usable after a cold tombstone");
    HoldOrdering once;
    GestureState onceGesture;
    int launches=0;
    if(once.Start(500))onceGesture.Start();
    onceGesture.Select(11,12);
    if(once.Stop(501) && onceGesture.Release())++launches;
    if(once.Stop(501) && onceGesture.Release())++launches;
    Check(launches==1,"duplicate release packets cannot launch an agent twice");

    SetLanguage(Language::English);
    std::vector<AppTarget> presets;
    for(int i=0;i<13;++i)presets.push_back({L"Preset "+std::to_wstring(i),TargetKind::Shell,L"",L"",L""});
    for(size_t count=1;count<=8;++count) {
        std::vector<AppTarget> apps;
        for(size_t i=0;i<count;++i)apps.push_back({L"App "+std::to_wstring(i),TargetKind::Shell,
            L"C:\\Apps\\App"+std::to_wstring(i)+L".exe",L"",L""});
        const auto main=BuildMenu(MenuPage::Main,apps,presets);
        Check(main.size()==count+1 && main.size()<=9,"main wheel has exactly one combined plus entry");
        for(size_t i=0;i<count;++i) {
            Check(main[i].action==MenuAction::Launch && main[i].index==static_cast<int>(i) &&
                main[i].visual.target==apps[i].target,"launch sectors preserve target identity and index");
        }
        Check(main[count].action==MenuAction::Settings && main[count].visual.target==L"reel:add",
            "the single plus icon opens the combined hub without launching an app");
        const auto hub=BuildMenu(MenuPage::Settings,apps,presets);
        Check(hub.size()==(count<8?5u:4u),"hub reserves its Add action only while another app can be saved");
        const auto manage=BuildMenu(MenuPage::Manage,apps,presets);
        Check(manage.size()==count+(count<8?1:0),"manage wheel exposes Add only below the saved-app limit");
        for(size_t i=0;i<count;++i)
            Check(manage[i].action==MenuAction::Entry && manage[i].index==static_cast<int>(i),"manage opens the selected entry submenu");
        if(count<8)Check(manage.back().action==MenuAction::Presets,"Add opens the preset wheel");
        const auto preferred=BuildMenu(MenuPage::Preferred,apps,presets);
        Check(preferred.size()==count,"preferred wheel lists every saved app");
        for(size_t i=0;i<count;++i) {
            Check(preferred[i].action==MenuAction::SetPreferred && preferred[i].index==static_cast<int>(i),
                "preferred choice changes preference without launching");
            const auto entry=BuildMenu(MenuPage::Entry,apps,presets,0,static_cast<int>(i));
            Check(entry.size()==(count>1?3u:2u) && entry[0].action==MenuAction::Edit &&
                entry[1].action==MenuAction::SetPreferred,"entry wheel provides edit and preferred actions");
            for(const auto& command:entry)Check(command.index==static_cast<int>(i) && command.action!=MenuAction::Launch,
                "entry submenu preserves its app index and cannot launch");
            if(count>1)Check(entry[2].action==MenuAction::Remove,"remove is available only while another app remains");
        }
    }
    const std::vector<AppTarget> noApps;
    const auto settings=BuildMenu(MenuPage::Settings,noApps,presets);
    Check(settings.size()==5 && settings[0].action==MenuAction::Language &&
        settings[1].action==MenuAction::Appearance && settings[2].action==MenuAction::Preferred &&
        settings[3].action==MenuAction::Manage && settings[4].action==MenuAction::Presets,
        "the combined hub contains language, appearance, preferred agent, management, and Add");
    for(const auto& command:settings)Check(command.visual.target.starts_with(L"reel:") && command.action!=MenuAction::Launch,
        "settings metadata cannot be mistaken for an app-launch action");
    for(size_t offset : {0u,5u,10u}) {
        const auto page=BuildMenu(MenuPage::Presets,noApps,presets,offset);
        const size_t shown=std::min(size_t{5},presets.size()-offset);
        Check(page.size()==shown+1+(offset>0?1:0)+(offset+5<presets.size()?1:0) && page.size()<=8,
            "preset page budgets room for custom and navigation sectors");
        for(size_t i=0;i<shown;++i)Check(page[i].action==MenuAction::Preset &&
            page[i].index==static_cast<int>(offset+i) && page[i].visual.target.empty(),
            "preset page preserves absolute indices and missing desktop targets");
        Check(page[shown].action==MenuAction::Custom,"custom executable selection is always available");
        size_t navigation=shown+1;
        if(offset>0)Check(page[navigation++].action==MenuAction::Previous,"later preset pages provide a previous route");
        if(offset+5<presets.size())Check(page[navigation].action==MenuAction::Next,"nonfinal preset pages provide a next route");
    }
    Check(BuildMenu(MenuPage::Presets,noApps,noApps).size()==1,"empty preset catalog still permits a custom app");
    SetLanguage(Language::SimplifiedChinese);
    const auto translatedSettings=BuildMenu(MenuPage::Settings,noApps,presets);
    Check(translatedSettings[0].visual.name!=settings[0].visual.name &&
        translatedSettings[0].action==settings[0].action && translatedSettings[0].visual.target==settings[0].visual.target,
        "language changes labels without changing action identity");
    SetLanguage(Language::English);
    Check(ParseUri(L"copilot-reel://?state=Down")==Command::HoldStart, "protocol down");
    Check(ParseUri(L"copilot-reel://?state=Up")==Command::HoldStop, "protocol up");
    Check(ParseUri(L"copilot-reel://?state=Tap")==Command::Tap, "protocol tap");
    Check(ParseUri(L"copilot-reel://?state=Up&target=evil.exe")==Command::Invalid, "URI cannot inject target");
    Check(ParseUri(L"https://example.com/?state=Down")==Command::Invalid, "foreign URI rejected");
    std::cout << "Core tests passed: 1-12 sectors, pointer-centered placement at 96-384 DPI and negative monitor origins, cold gesture ordering, exactly-once release, radial routes, preset pagination, and protocol validation.\n";
}
