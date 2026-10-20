#include "app_types.h"
#include "appearance.h"
#include "core.h"
#include "renderer.h"
#include "settings.h"
#include "localization.h"
#include "osd.h"
#include "behavior.h"
#include "presets.h"
#include "official_icons.h"
#include "radial_menu.h"
#include "local_launch.h"
#include "display_names.h"
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <propsys.h>
#include <sddl.h>
#include <wtsapi32.h>
#include <commctrl.h>
#include <commdlg.h>
#include <imm.h>
#include <winrt/base.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

namespace reel {
namespace {
constexpr UINT kSmokeTimer=101, kIdleTimer=102, kCloseTimer=103;
constexpr UINT kCommandPreview=201,kCommandSettings=202,kCommandDiagnostics=203,kCommandExit=204;
constexpr ULONG_PTR kIpcMagic=0x5245454c;
constexpr PROPERTYKEY kFastPathKey={{0x38652bca,0x4329,0x4e74,{0x86,0xf9,0x39,0xcf,0x29,0x34,0x5e,0xea}},2};

struct CommandPacket { unsigned version=1; Command command=Command::Invalid; uint64_t stamp=0; POINT pointer{}; };
uint64_t EventStamp(bool processCreation=false) {
    FILETIME time{},exit{},kernel{},user{};
    if(!processCreation || !GetProcessTimes(GetCurrentProcess(),&time,&exit,&kernel,&user))
        GetSystemTimePreciseAsFileTime(&time);
    return (static_cast<uint64_t>(time.dwHighDateTime)<<32)|time.dwLowDateTime;
}
CommandPacket CaptureCommand(Command command,bool cold=false) {
    CommandPacket result;result.command=command;result.stamp=EventStamp(cold);GetCursorPos(&result.pointer);return result;
}

std::wstring DataDirectory() {
    const auto path=std::filesystem::path(ConfigPath()).parent_path();
    if(path.empty())throw std::runtime_error("The application data path is unavailable");
    std::filesystem::create_directories(path);return path.wstring();
}
void Log(const std::wstring& value) {
    try {
        SYSTEMTIME time{}; GetLocalTime(&time);
        wchar_t prefix[80]{}; swprintf_s(prefix,L"%04u-%02u-%02u %02u:%02u:%02u.%03u ",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds);
        const auto text=winrt::to_string(std::wstring(prefix)+value+L"\r\n");
        HANDLE file=CreateFileW((DataDirectory()+L"\\run.log").c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file!=INVALID_HANDLE_VALUE) { DWORD written=0; WriteFile(file,text.data(),static_cast<DWORD>(text.size()),&written,nullptr); CloseHandle(file); }
    } catch(...) {}
}
std::wstring UserSuffix() {
    HANDLE token=nullptr;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) return L"CurrentUser";
    DWORD bytes=0; GetTokenInformation(token,TokenUser,nullptr,0,&bytes);
    std::vector<BYTE> buffer(bytes);
    std::wstring result=L"CurrentUser";
    if(GetTokenInformation(token,TokenUser,buffer.data(),bytes,&bytes)) {
        LPWSTR sid=nullptr;
        if(ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid,&sid)) {result=sid;LocalFree(sid);}
    }
    CloseHandle(token); return result;
}

Command CommandLine() {
    int count=0; auto args=CommandLineToArgvW(GetCommandLineW(),&count);
    Command result=Command::Preview;
    if(count==2) {
        const std::wstring_view arg=args[1];
        if(arg==L"--background") result=Command::Background;
        else if(arg==L"--preview") result=Command::Preview;
        else if(arg==L"--tap") result=Command::Tap;
        else if(arg==L"--settings") result=Command::Settings;
        else if(arg==L"--hold-start") result=Command::HoldStart;
        else if(arg==L"--hold-stop") result=Command::HoldStop;
        else if(arg==L"--cancel") result=Command::Cancel;
        else if(arg==L"--quit") result=Command::Quit;
        else if(arg==L"--diagnostics") result=Command::Diagnostics;
        else if(arg==L"--smoke-test") result=Command::SmokeTest;
        else result=Command::Invalid;
    } else if(count==3 && std::wstring_view(args[1])==L"--copilot-uri") result=ParseUri(args[2]);
    else if(count!=1) result=Command::Invalid;
    LocalFree(args); return result;
}

bool RegisterFastPath(HWND hwnd) {
    winrt::com_ptr<IPropertyStore> store;
    HRESULT hr=SHGetPropertyStoreForWindow(hwnd,IID_PPV_ARGS(store.put()));
    if(FAILED(hr)) return false;
    PROPVARIANT value{};value.vt=VT_UINT;value.uintVal=kCopilotMessage;
    hr=store->SetValue(kFastPathKey,value);
    return SUCCEEDED(hr) && SUCCEEDED(store->Commit());
}

void Launch(const AppTarget& target,HWND owner) {
    Log(L"Launching target: "+target.name);
    HRESULT hr=S_OK;
    if(target.kind==TargetKind::Aumid) {
        winrt::com_ptr<IApplicationActivationManager> manager;
        hr=CoCreateInstance(CLSID_ApplicationActivationManager,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(manager.put()));
        DWORD pid=0;
        if(SUCCEEDED(hr)) hr=manager->ActivateApplication(target.target.c_str(),target.arguments.c_str(),AO_NONE,&pid);
    } else {
        SHELLEXECUTEINFOW info{sizeof(info)};
        info.fMask=SEE_MASK_FLAG_NO_UI|SEE_MASK_NOASYNC;
        info.hwnd=owner;info.lpVerb=L"open";info.lpFile=target.target.c_str();
        info.lpParameters=target.arguments.empty()?nullptr:target.arguments.c_str();
        info.lpDirectory=target.workingDirectory.empty()?nullptr:target.workingDirectory.c_str();
        info.nShow=SW_SHOWNORMAL;
        if(!ShellExecuteExW(&info)) hr=HRESULT_FROM_WIN32(GetLastError());
    }
    if(FAILED(hr)) {
        wchar_t number[40]{};swprintf_s(number,L"0x%08X",static_cast<unsigned>(hr));
        Log(L"Launch failed "+std::wstring(number));
        MessageBoxW(nullptr,(std::wstring(Tr(L"Could not open: ",L"无法打开："))+AppDisplayName(target)+Tr(L"\nCheck the target and arguments in App settings.\n\n",L"\n请检查应用设置中的目标与参数。\n\n")+number).c_str(),kAppName,MB_OK|MB_ICONERROR);
    }
}

class Controller {
public:
    HWND window=nullptr;
    bool smoke=false;
    explicit Controller(bool isSmoke):smoke(isSmoke) {}
    ~Controller() {
        appearance_.reset(); renderer_.reset();
        if(window) { WTSUnRegisterSessionNotification(window); RemoveTray(); }
    }
    bool Initialize() {
        // The reel accepts commands, not text. Leave the separate native editor's
        // IME intact while ensuring A/D also work under Chinese input layouts.
        ImmAssociateContextEx(window,nullptr,0);
        appearance_=std::make_unique<AppearanceSource>(window);
        renderer_=std::make_unique<OrbitRenderer>(window);
        std::wstring error;
        if(!renderer_->Initialize(error)) {
            Log(L"Renderer initialization failed: "+error);
            MessageBoxW(nullptr,(std::wstring(Tr(L"Could not initialize native Windows rendering.\n",L"无法初始化 Windows 原生绘制。\n"))+error).c_str(),kAppName,MB_OK|MB_ICONERROR); return false;
        }
        WTSRegisterSessionNotification(window,NOTIFY_FOR_THIS_SESSION);
        PreloadOfficialIconAssets();
        presets_=AgentPresets();
        RefreshAppearance(); Reload(); AddTray();
        // This ordinary optional shortcut opens click-to-select preview; it never intercepts Copilot.
        hotkey_=RegisterHotKey(window,1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,'R')!=FALSE;
        ready_=true;
        RegisterFastPath(window);
        Log(L"Ready; fast-path HWND registered; Ctrl+Alt+R "+std::wstring(hotkey_?L"available":L"unavailable"));
        return true;
    }
    void CommandReceived(Command command) { CommandReceived(CaptureCommand(command)); }
    bool QueueCommand(const CommandPacket& event) {
        pending_.push_back(event);
        if(PostMessageW(window,WM_APP+20,0,0))return true;
        pending_.pop_back();return false;
    }
    void DispatchQueued() {
        if(pending_.empty())return;
        const auto event=pending_.front();pending_.pop_front();CommandReceived(event);
    }
    void CommandReceived(const CommandPacket& event) {
        switch(event.command) {
        case Command::Background: break;
        case Command::Settings: Cancel();Start(true,event.pointer,MenuPage::Settings);break;
        case Command::Preview: Start(true,event.pointer);break;
        case Command::Tap: {
            Cancel();const int preferred=PreferredTargetIndex(items_);
            if(preferred>=0) {
                if(IsVerifiedLocalApplication(items_[preferred]))LaunchTarget(items_[preferred]);
                else ChooseLocalApplication(preferred,true);
            }break;
        }
        case Command::HoldStart:
            if(!gesture_.Active() && ordering_.Start(event.stamp))Start(false,event.pointer);break;
        case Command::HoldStop:
            if(ordering_.Stop(event.stamp) && !preview_)Release();break;
        case Command::Cancel: Cancel();break;
        case Command::Quit:
            quitting_=true;
            if(CloseOsd())DestroyWindow(window);else quitting_=false;break;
        case Command::Diagnostics: SaveDiagnostics();break;
        case Command::SmokeTest: BeginSmoke();break;
        default: break;
        }
    }
    void Start(bool preview,POINT pointer,MenuPage page=MenuPage::Main) {
        if(gesture_.Active() || editorOpen_)return;
        KillTimer(window,kCloseTimer);closing_=false;
        previousForeground_=GetForegroundWindow();
        preview_=preview;page_=page;anchor_=pointer;
        const HMONITOR monitor=MonitorFromPoint(anchor_,MONITOR_DEFAULTTONEAREST);
        UINT xDpi=96,yDpi=96;
        using GetDpiForMonitorFn=HRESULT(WINAPI*)(HMONITOR,int,UINT*,UINT*);
        HMODULE shcore=LoadLibraryExW(L"shcore.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(shcore) {
            auto getDpi=reinterpret_cast<GetDpiForMonitorFn>(GetProcAddress(shcore,"GetDpiForMonitor"));
            if(getDpi)getDpi(monitor,0,&xDpi,&yDpi);
            FreeLibrary(shcore);
        }
        dpi_=xDpi;
        MONITORINFO info{sizeof(info)};if(!GetMonitorInfoW(monitor,&info))return;
        const auto& work=info.rcWork;
        int size=MulDiv(static_cast<int>(kCanvasDip),dpi_,96);
        // A tiny work area reduces the complete canvas uniformly, never opens a
        // different settings dashboard and never places buttons off screen.
        if(size>work.right-work.left || size>work.bottom-work.top) {
            size=std::min(work.right-work.left,work.bottom-work.top);
            dpi_=std::max(1,MulDiv(size,96,static_cast<int>(kCanvasDip)));
            size=MulDiv(static_cast<int>(kCanvasDip),dpi_,96);
        }
        const auto placement=PlaceOrbit(anchor_.x,anchor_.y,{work.left,work.top,work.right,work.bottom},size,kDeadZoneDip*dpi_/96.0);
        if(!placement.fits)return;
        centerX_=placement.centerX;centerY_=placement.centerY;
        placing_=true;
        SetWindowPos(window,HWND_TOPMOST,placement.left,placement.top,size,size,SWP_NOACTIVATE);
        placing_=false;
        gesture_.Start(!preview && placement.awaitingCenter);
        renderer_->Resize(dpi_);renderer_->SetAppearance(currentAppearance_);
        renderer_->SetAnchorOffset(static_cast<float>((anchor_.x-centerX_)*96/dpi_),
            static_cast<float>((anchor_.y-centerY_)*96/dpi_));
        languageFocus_=page==MenuPage::Settings;
        RenderMenu();
        ShowWindow(window,SW_SHOWNOACTIVATE);renderer_->Show();
        RegisterRawInput(true);
        if(preview)ActivateMenu();
        SetTimer(window,kIdleTimer,preview?60000:60000,nullptr);
        RECT bounds{};GetWindowRect(window,&bounds);
        Log(std::wstring(preview?L"Preview opened":L"Hold started")+L"; pointer="+std::to_wstring(anchor_.x)+L","+std::to_wstring(anchor_.y)+L"; window="+std::to_wstring(bounds.left)+L","+std::to_wstring(bounds.top)+L","+std::to_wstring(bounds.right)+L","+std::to_wstring(bounds.bottom)+L"; awaitingCenter="+(gesture_.AwaitingCenter()?L"true":L"false"));
    }
    void ActivateMenu() {
        SetWindowLongPtrW(window,GWL_EXSTYLE,GetWindowLongPtrW(window,GWL_EXSTYLE)&~WS_EX_NOACTIVATE);
        SetForegroundWindow(window);SetFocus(window);
    }
    std::wstring MenuTitle() const {
        switch(page_) {
        case MenuPage::Manage:return Tr(L"Your apps",L"管理应用");
        case MenuPage::Presets:return Tr(L"Add an agent",L"添加应用");
        case MenuPage::Settings:return Tr(L"Settings",L"设置");
        case MenuPage::Preferred:return Tr(L"Preferred agent",L"首选应用");
        case MenuPage::Entry:return editing_>=0 && static_cast<size_t>(editing_)<items_.size()?AppDisplayName(items_[editing_]):L"";
        default:return L"Copilot Reel";
        }
    }
    void RenderMenu() {
        if(page_==MenuPage::Entry && (editing_<0 || static_cast<size_t>(editing_)>=items_.size()))page_=MenuPage::Manage;
        menu_=BuildMenu(page_,items_,presets_,presetOffset_,editing_);
        std::vector<AppTarget> visuals;for(const auto& entry:menu_)visuals.push_back(entry.visual);
        renderer_->SetItems(visuals);
        renderer_->SetPreferredIndex(page_==MenuPage::Main || page_==MenuPage::Preferred || page_==MenuPage::Manage?PreferredTargetIndex(items_):-1);
        renderer_->SetMenuContext(MenuTitle(),page_!=MenuPage::Main,languageFocus_);
        renderer_->SetAwaitCenter(gesture_.AwaitingCenter());renderer_->SetSelection(gesture_.Selected());
        Log(L"Radial menu: "+MenuTitle()+L"; entries="+std::to_wstring(menu_.size()));
    }
    void Navigate(MenuPage page) {
        page_=page;preview_=true;languageFocus_=page==MenuPage::Settings;
        gesture_.Cancel();gesture_.Start();
        RenderMenu();ActivateMenu();SetTimer(window,kIdleTimer,60000,nullptr);
    }
    void Back() {
        if(!gesture_.Active())return;
        switch(page_) {
        case MenuPage::Presets:Navigate(presetsParent_);break;
        case MenuPage::Entry:Navigate(MenuPage::Manage);break;
        case MenuPage::Manage:case MenuPage::Preferred:Navigate(MenuPage::Settings);break;
        case MenuPage::Main:Cancel();break;
        default:Navigate(MenuPage::Main);break;
        }
    }
    POINT LocalPointer() const {
        POINT point{};GetCursorPos(&point);ScreenToClient(window,&point);
        point.x=MulDiv(point.x,96,dpi_);point.y=MulDiv(point.y,96,dpi_);return point;
    }
    bool OnLanguageArrow(POINT point) const {
        if(!languageFocus_)return false;
        auto bounds=renderer_->LanguageArrowBounds();
        for(const auto& r:{bounds.first,bounds.second})
            if(point.x>=r.left && point.x<=r.right && point.y>=r.top && point.y<=r.bottom)return true;
        return false;
    }
    void MovePointer() {
        if(!gesture_.Active())return;
        const POINT local=LocalPointer();
        if(OnLanguageArrow(local)) {
            // Both arrows belong to the language item, including its hover name.
            if(gesture_.Select(0,static_cast<unsigned>(menu_.size())))renderer_->SetSelection(0);
            return;
        }
        POINT p{};GetCursorPos(&p);
        const bool wasAwaiting=gesture_.AwaitingCenter();
        if(gesture_.ObservePointer((p.x-centerX_)*96.0/dpi_,(p.y-centerY_)*96.0/dpi_,static_cast<unsigned>(menu_.size()))) {
            renderer_->SetAwaitCenter(gesture_.AwaitingCenter());renderer_->SetSelection(gesture_.Selected());
            if(wasAwaiting && !gesture_.AwaitingCenter())Log(L"Center entered; direction selection armed");
        }
    }
    void SwitchLanguage() {
        const auto language=CurrentLanguage()==Language::English?Language::SimplifiedChinese:Language::English;
        if(!SaveLanguage(language)) {Log(L"Language save failed");return;}
        SetLanguage(language);RenderMenu();RemoveTray();AddTray();RefreshOsdAppearance();
        Log(language==Language::English?L"Language changed: English":L"Language changed: Simplified Chinese");
    }
    void KeyDown(WPARAM key) {
        if(!preview_ || !gesture_.Active())return;
        if(key==VK_ESCAPE || key==VK_BACK) {Back();return;}
        if(page_==MenuPage::Settings && (key=='A'||key=='D'||key==VK_LEFT||key==VK_RIGHT)) {
            languageFocus_=true;gesture_.Select(0,static_cast<unsigned>(menu_.size()));SwitchLanguage();return;
        }
        if(key==VK_TAB || key==VK_LEFT || key==VK_RIGHT) {
            const int count=static_cast<int>(menu_.size());if(!count)return;
            const int delta=key==VK_LEFT?-1:1;
            const int selected=gesture_.Selected();
            gesture_.Select(selected<0?(delta<0?count-1:0):(selected+delta+count)%count,count);
            languageFocus_=page_==MenuPage::Settings;
            renderer_->SetSelection(gesture_.Selected());renderer_->SetMenuContext(MenuTitle(),page_!=MenuPage::Main,languageFocus_);
        }
        if(key==VK_RETURN || key==VK_SPACE) {
            const int index=gesture_.Selected();if(index>=0)Choose(index);
        }
    }
    void Click() {
        if(!preview_ || !gesture_.Active())return;
        const auto point=LocalPointer();
        if(OnLanguageArrow(point)) {SwitchLanguage();return;}
        const int selected=HitTest(point.x-kCenterDip,point.y-kCenterDip,static_cast<unsigned>(menu_.size()));
        if(selected<0)Back();else Choose(selected);
    }
    void Release() {
        if(!gesture_.Active())return;
        MovePointer();const auto selected=gesture_.Release();
        if(selected)Choose(*selected);else {Hide();Log(L"Gesture canceled before selection");}
    }
    void LaunchTarget(const AppTarget& target) {
        if(smoke)Log(L"Smoke simulated launch: "+target.name);else Launch(target,window);
    }
    bool PersistLocalTargets(const std::vector<AppTarget>& candidate,int preferred=-1) {
        const auto path=ConfigPath();
        if(GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES) {
            const auto backup=path+L".before-local-apps."+std::to_wstring(EventStamp())+L".bak";
            if(!CopyFileW(path.c_str(),backup.c_str(),TRUE)) {Log(L"Local app migration backup failed");return false;}
        }
        std::wstring error;
        if(!SaveTargets(candidate,error)) {Log(L"Local app configuration save failed");return false;}
        if(preferred<0)preferred=PreferredTargetIndex(items_);
        if(preferred>=0 && static_cast<size_t>(preferred)<candidate.size() && !SavePreferredTarget(candidate[preferred])) {
            std::wstring rollbackError;
            if(!SaveTargets(items_,rollbackError))Log(L"Preference save and configuration rollback failed; backup retained");
            else Log(L"Preference save failed; original configuration restored");
            return false;
        }
        items_=candidate;
        return true;
    }
    void ChooseLocalApplication(int index,bool launchAfterSelection) {
        if(index<0 || static_cast<size_t>(index)>=items_.size() || editorOpen_)return;
        gesture_.Cancel();Hide(false);FinishHide();
        std::array<wchar_t,32768> path{};
        OPENFILENAMEW picker{sizeof(picker)};picker.hwndOwner=window;
        picker.lpstrTitle=Tr(L"Choose a local application",L"选择本机应用");
        picker.lpstrFilter=Tr(L"Applications and shortcuts\0*.exe;*.lnk\0\0",L"应用程序和快捷方式\0*.exe;*.lnk\0\0");
        picker.lpstrFile=path.data();picker.nMaxFile=static_cast<DWORD>(path.size());
        picker.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_NODEREFERENCELINKS;
        editorOpen_=true;
        const auto previousStyle=GetWindowLongPtrW(window,GWL_EXSTYLE);
        SetWindowLongPtrW(window,GWL_EXSTYLE,previousStyle&~WS_EX_NOACTIVATE);
        Log(L"Short tap requires a local application: "+items_[index].name);
        const bool chosen=GetOpenFileNameW(&picker)!=FALSE;
        SetWindowLongPtrW(window,GWL_EXSTYLE,previousStyle);
        editorOpen_=false;
        if(chosen) {
            auto candidate=items_;
            candidate[index]={items_[index].name,TargetKind::Shell,path.data(),L"",L""};
            if(IsVerifiedLocalApplication(candidate[index]) && PersistLocalTargets(candidate,index)) {
                if(launchAfterSelection)LaunchTarget(items_[index]);else Start(true,anchor_,MenuPage::Settings);
            } else MessageBoxW(nullptr,Tr(L"Could not save this local application. Choose an executable or a shortcut to an installed application.",
                L"无法保存此本机应用。请选择可执行程序或指向已安装应用的快捷方式。"),kAppName,MB_OK|MB_ICONWARNING);
        } else if(!launchAfterSelection)Start(true,anchor_,MenuPage::Preferred);
    }
    void Choose(int selected) {
        if(selected<0 || static_cast<size_t>(selected)>=menu_.size())return;
        const auto entry=menu_[selected];
        switch(entry.action) {
        case MenuAction::Launch:gesture_.Cancel();Hide(false);LaunchTarget(entry.visual);Log(L"Gesture committed");break;
        case MenuAction::Manage:Navigate(MenuPage::Manage);break;
        case MenuAction::Settings:Navigate(MenuPage::Settings);break;
        case MenuAction::Language:SwitchLanguage();break;
        case MenuAction::Preferred:Navigate(MenuPage::Preferred);break;
        case MenuAction::Appearance:
            gesture_.Cancel();Hide(false);LaunchTarget({L"Windows appearance",TargetKind::Shell,L"ms-settings:colors",L"",L""});break;
        case MenuAction::Entry:editing_=entry.index;Navigate(MenuPage::Entry);break;
        case MenuAction::Edit:OpenEditor(entry.index);break;
        case MenuAction::SetPreferred:
            if(entry.index>=0 && static_cast<size_t>(entry.index)<items_.size()) {
                if(!IsVerifiedLocalApplication(items_[entry.index]))ChooseLocalApplication(entry.index,false);
                else if(SavePreferredTarget(items_[entry.index])) {
                    Log(L"Preferred agent saved: "+items_[entry.index].name);Navigate(MenuPage::Settings);
                }
            }break;
        case MenuAction::Remove:
            if(entry.index>=0 && static_cast<size_t>(entry.index)<items_.size() && items_.size()>1) {
                auto candidate=items_;candidate.erase(candidate.begin()+entry.index);std::wstring error;
                if(SaveTargets(candidate,error)){items_=std::move(candidate);Navigate(MenuPage::Manage);}
                else MessageBoxW(window,error.c_str(),kAppName,MB_OK|MB_ICONERROR);
            }break;
        case MenuAction::Presets:presetsParent_=page_;presetOffset_=0;Navigate(MenuPage::Presets);break;
        case MenuAction::Previous:presetOffset_=presetOffset_>=5?presetOffset_-5:0;Navigate(MenuPage::Presets);break;
        case MenuAction::Next:presetOffset_+=5;Navigate(MenuPage::Presets);break;
        case MenuAction::Custom:OpenEditor(-1);break;
        case MenuAction::Preset:
            if(entry.index>=0 && static_cast<size_t>(entry.index)<presets_.size() && items_.size()<8) {
                const auto preset=presets_[entry.index];
                if(preset.target.empty())OpenEditor(-1,&preset);
                else {
                    auto candidate=items_;candidate.push_back(preset);std::wstring error;
                    if(SaveTargets(candidate,error)) {items_=std::move(candidate);Navigate(MenuPage::Manage);}
                    else MessageBoxW(window,error.c_str(),kAppName,MB_OK|MB_ICONERROR);
                }
            }break;
        }
    }
    void OpenEditor(int index,const AppTarget* preset=nullptr) {
        gesture_.Cancel();Hide(false);FinishHide();editorOpen_=true;
        if(preset)ShowPresetTargetEditor(window,*preset);else ShowTargetEditor(window,index);
    }
    void EditorClosed() {
        editorOpen_=false;if(quitting_)return;
        std::wstring error;items_=LoadTargets(error);
        Start(true,anchor_,MenuPage::Manage);
    }
    void Cancel() {
        if(!gesture_.Active())return;
        gesture_.Cancel();Hide();Log(L"Canceled");
    }
    void Hide(bool restoreFocus=true) {
        KillTimer(window,kIdleTimer);RegisterRawInput(false);preview_=false;
        if(restoreFocus && GetForegroundWindow()==window && IsWindow(previousForeground_))SetForegroundWindow(previousForeground_);
        SetWindowLongPtrW(window,GWL_EXSTYLE,GetWindowLongPtrW(window,GWL_EXSTYLE)|WS_EX_NOACTIVATE);
        const unsigned duration=renderer_->BeginHide();closing_=true;
        if(duration)SetTimer(window,kCloseTimer,duration,nullptr);else FinishHide();
    }
    void FinishHide() {
        KillTimer(window,kCloseTimer);closing_=false;renderer_->Hide();ShowWindow(window,SW_HIDE);
    }
    void RefreshAppearance() {
        if(!appearance_ || !renderer_)return;
        MSG ignored{};
        while(PeekMessageW(&ignored,window,kAppearanceMessage,kAppearanceMessage,PM_REMOVE)) {}
        currentAppearance_=appearance_->Read();
        renderer_->SetAppearance(currentAppearance_);
        RefreshOsdAppearance();
        Log(L"Appearance: "+std::wstring(currentAppearance_.dark?L"dark":L"light")+L", transparency="+(currentAppearance_.transparency?L"on":L"off"));
    }
    void Reload() {
        const bool firstRun=GetFileAttributesW(ConfigPath().c_str())==INVALID_FILE_ATTRIBUTES;
        std::wstring error;items_=LoadTargets(error);
        if(error.empty()) {
            auto candidate=items_;
            bool changed=false;
            if(firstRun) {
                candidate.clear();
                for(const auto& preset:presets_)if(IsVerifiedLocalApplication(preset) && candidate.size()<6)candidate.push_back(preset);
                candidate.insert(candidate.end(),items_.begin(),items_.end());
                changed=candidate.size()!=items_.size();
            } else {
                for(auto& item:candidate)if(!IsLocalApplication(item)) {
                    if(auto replacement=LocalReplacement(item,presets_);replacement && IsVerifiedLocalApplication(*replacement)) {item=*replacement;changed=true;}
                }
            }
            if(changed && PersistLocalTargets(candidate))Log(L"Local application profiles saved; previous configuration backed up");
        }
        if(gesture_.Active())Navigate(page_);else RenderMenu();
        if(!error.empty()) {
            Log(L"Configuration validation failed; original file preserved, fallback targets loaded.");
            // Never block initialization in a modal message loop. Settings shows the full error.
            NOTIFYICONDATAW notice{sizeof(notice)};notice.hWnd=window;notice.uID=1;notice.uFlags=NIF_INFO;
            wcscpy_s(notice.szInfoTitle,Tr(L"Copilot Reel configuration needs attention",L"Copilot Reel 配置需要检查"));
            wcscpy_s(notice.szInfo,Tr(L"The original file is preserved. Default targets are temporarily loaded. Open App settings to review the error.",L"原配置已保留，暂时使用默认入口。请从托盘打开应用设置查看错误。"));
            notice.dwInfoFlags=NIIF_WARNING;Shell_NotifyIconW(NIM_MODIFY,&notice);
        }
    }
    void RegisterRawInput(bool enable) {
        RAWINPUTDEVICE devices[2]{};
        // Keyboard raw input only supplies Escape in this app's visible gesture. No key logging.
        for(int i=0;i<2;++i) { devices[i].usUsagePage=1;devices[i].usUsage=i==0?2:6;devices[i].dwFlags=enable?RIDEV_INPUTSINK:RIDEV_REMOVE;devices[i].hwndTarget=enable?window:nullptr; }
        if(!RegisterRawInputDevices(devices,2,sizeof(RAWINPUTDEVICE))) Log(L"Raw Input registration failed");
    }
    void RawInput(HRAWINPUT input) {
        UINT size=0;
        if(GetRawInputData(input,RID_INPUT,nullptr,&size,sizeof(RAWINPUTHEADER))!=0 || size>65536) return;
        std::vector<BYTE> data(size);
        if(GetRawInputData(input,RID_INPUT,data.data(),&size,sizeof(RAWINPUTHEADER))!=size) return;
        auto raw=reinterpret_cast<const RAWINPUT*>(data.data());
        if(raw->header.dwType==RIM_TYPEMOUSE) MovePointer();
        else if(!preview_ && raw->header.dwType==RIM_TYPEKEYBOARD && raw->data.keyboard.VKey==VK_ESCAPE && !(raw->data.keyboard.Flags&RI_KEY_BREAK)) Cancel();
    }
    void AddTray() {
        NOTIFYICONDATAW data{sizeof(data)};data.hWnd=window;data.uID=1;
        data.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;data.uCallbackMessage=kTrayMessage;
        data.hIcon=LoadIconW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1));
        if(!data.hIcon) data.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
        wcscpy_s(data.szTip,Tr(L"Copilot Reel - Ctrl+Alt+R to preview",L"Copilot Reel · Ctrl+Alt+R 预览"));
        Shell_NotifyIconW(NIM_ADD,&data);data.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&data);
    }
    void RemoveTray() {NOTIFYICONDATAW data{sizeof(data)};data.hWnd=window;data.uID=1;Shell_NotifyIconW(NIM_DELETE,&data);}
    void TrayMenu() {
        HMENU menu=CreatePopupMenu();
        AppendMenuW(menu,MF_STRING,kCommandPreview,Tr(L"Preview reel (click to open)",L"预览轮盘（点击打开）"));
        AppendMenuW(menu,MF_STRING,kCommandSettings,Tr(L"Quick menu...",L"快捷菜单…"));
        AppendMenuW(menu,MF_STRING,kCommandDiagnostics,Tr(L"Write diagnostics",L"写出诊断信息"));
        AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
        AppendMenuW(menu,MF_STRING,kCommandExit,Tr(L"Exit",L"退出"));
        POINT p{};GetCursorPos(&p);SetForegroundWindow(window);
        const UINT choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,window,nullptr);
        PostMessageW(window,WM_NULL,0,0);DestroyMenu(menu);
        if(choice) PostMessageW(window,WM_COMMAND,choice,0);
    }
    void SaveDiagnostics() {
        std::wostringstream text;
        text<<L"Copilot Reel 0.3.3\r\n"<<L"DPI="<<dpi_<<L"\r\n"<<L"dark="<<currentAppearance_.dark<<L"\r\ntransparencyRequested="<<currentAppearance_.transparency
            <<L"\r\nanimations="<<currentAppearance_.animations<<L"\r\nhighContrast="<<currentAppearance_.highContrast<<L"\r\ntargetCount="<<items_.size()
            <<L"\r\nmenuPage="<<static_cast<int>(page_)<<L"\r\nmenuEntries="<<menu_.size()<<L"\r\npresetOffset="<<presetOffset_
            <<L"\r\npreferredIndex="<<PreferredTargetIndex(items_)<<L"\r\ngestureActive="<<gesture_.Active()
            <<L"\r\npreview="<<preview_<<L"\r\neditorOpen="<<editorOpen_
            <<L"\r\ndataPath="<<ConfigPath()<<L"\r\nlanguage="<<(CurrentLanguage()==Language::English?L"en-US":L"zh-CN")
            <<L"\r\n"<<renderer_->Diagnostics();
        const auto bytes=winrt::to_string(text.str());
        const auto path=DataDirectory()+L"\\diagnostics.txt";
        HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file!=INVALID_HANDLE_VALUE) {DWORD written=0;WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr);CloseHandle(file);}
        Log(L"Diagnostics written");
    }
    void BeginSmoke() {
        smokeStep_=0;POINT pointer{};GetCursorPos(&pointer);Start(true,pointer);SetTimer(window,kSmokeTimer,1000,nullptr);
    }
    void SmokeStep() {
        ++smokeStep_;
        if(smokeStep_==1) {gesture_.Select(1,static_cast<unsigned>(items_.size()));renderer_->SetSelection(1);SaveDiagnostics();}
        if(smokeStep_==2) {auto a=currentAppearance_;a.transparency=false;renderer_->SetAppearance(a);}
        if(smokeStep_==3) {RefreshAppearance();}
        if(smokeStep_==4) {Cancel();SaveDiagnostics();KillTimer(window,kSmokeTimer);Log(L"Smoke test completed");DestroyWindow(window);}
    }
    bool IsPreview() const {return preview_;}
    bool Ready() const {return ready_;}
    bool Active() const {return gesture_.Active();}
    UINT Dpi() const {return dpi_;}
    void DpiChanged(UINT dpi) {
        if(placing_)return;
        if(renderer_ && gesture_.Active()) {Cancel();FinishHide();}
        dpi_=dpi;if(renderer_)renderer_->Resize(dpi_);
    }
private:
    std::unique_ptr<AppearanceSource> appearance_;
    std::unique_ptr<OrbitRenderer> renderer_;
    Appearance currentAppearance_;
    std::vector<AppTarget> items_,presets_;
    std::vector<MenuEntry> menu_;
    MenuPage page_=MenuPage::Main;
    MenuPage presetsParent_=MenuPage::Manage;
    size_t presetOffset_=0;
    int editing_=-1;
    bool languageFocus_=false,closing_=false,editorOpen_=false,quitting_=false,placing_=false;
    HWND previousForeground_=nullptr;
    HoldOrdering ordering_;
    std::deque<CommandPacket> pending_;
    GestureState gesture_;
    POINT anchor_{};
    double centerX_=0,centerY_=0;
    UINT dpi_=96;
    bool preview_=false,hotkey_=false,ready_=false;
    int smokeStep_=0;
};

LRESULT CALLBACK WindowProcedure(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
    auto self=reinterpret_cast<Controller*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(message==WM_NCCREATE) {
        self=static_cast<Controller*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        self->window=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
    }
    if(!self) return DefWindowProcW(hwnd,message,wParam,lParam);
    try {
        static const UINT taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
        if(message==taskbarCreated) {self->AddTray();return 0;}
        switch(message) {
        case kCopilotMessage:
            if(!self->Ready()) return 0;
            if(wParam==0) self->CommandReceived(Command::Tap);
            else if(wParam==1) self->CommandReceived(Command::HoldStart);
            else if(wParam==2) self->CommandReceived(Command::HoldStop);
            return 0;
        case WM_COPYDATA: {
            if(!self->Ready()) return FALSE;
            auto copy=reinterpret_cast<const COPYDATASTRUCT*>(lParam);
            if(!copy || copy->dwData!=kIpcMagic || copy->cbData!=sizeof(CommandPacket) || !copy->lpData) return FALSE;
            CommandPacket event;memcpy(&event,copy->lpData,sizeof(event));
            if(event.version!=1 || event.command>=Command::Invalid || !event.stamp) return FALSE;
            // Queue after acknowledging the client. No rendering or Shell activation inside SendMessage.
            return self->QueueCommand(event);
        }
        case WM_APP+20:self->DispatchQueued();return 0;
        case kAppearanceMessage:case WM_SETTINGCHANGE:case WM_THEMECHANGED:case WM_SYSCOLORCHANGE:self->RefreshAppearance();return 0;
        case kConfigChangedMessage:self->Reload();return 0;
        case kTargetEditorClosedMessage:self->EditorClosed();return 0;
        case kLanguageChangedMessage:self->RefreshAppearance();self->RemoveTray();self->AddTray();return 0;
        case WM_INPUT:self->RawInput(reinterpret_cast<HRAWINPUT>(lParam));break;
        case WM_MOUSEMOVE:self->MovePointer();return 0;
        case WM_LBUTTONUP:self->Click();return 0;
        case WM_RBUTTONUP:self->Back();return 0;
        case WM_KEYDOWN:if(!(lParam&(1LL<<30)))self->KeyDown(wParam);return 0;
        case WM_MOUSEACTIVATE:return self->IsPreview()?MA_ACTIVATE:MA_NOACTIVATE;
        case WM_NCHITTEST: {
            POINT p{GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)};ScreenToClient(hwnd,&p);
            const double x=p.x*96.0/self->Dpi()-kCenterDip,y=p.y*96.0/self->Dpi()-kCenterDip;
            return x*x+y*y<kRadiusDip*kRadiusDip?HTCLIENT:HTTRANSPARENT;
        }
        case WM_HOTKEY:if(wParam==1)self->CommandReceived(Command::Preview);return 0;
        case kTrayMessage:
            if(LOWORD(lParam)==WM_CONTEXTMENU) self->TrayMenu();
            else if(LOWORD(lParam)==NIN_SELECT || LOWORD(lParam)==NIN_KEYSELECT)self->CommandReceived(Command::Preview);
            return 0;
        case WM_COMMAND:
            switch(LOWORD(wParam)) {
            case kCommandPreview:self->CommandReceived(Command::Preview);break;
            case kCommandSettings:self->CommandReceived(Command::Settings);break;
            case kCommandDiagnostics:self->CommandReceived(Command::Diagnostics);break;
            case kCommandExit:self->CommandReceived(Command::Quit);break;
            }return 0;
        case WM_TIMER:
            if(wParam==kSmokeTimer)self->SmokeStep();
            else if(wParam==kIdleTimer)self->Cancel();
            else if(wParam==kCloseTimer)self->FinishHide();
            return 0;
        case WM_DPICHANGED:self->DpiChanged(HIWORD(wParam));return 0;
        case WM_WTSSESSION_CHANGE:if(wParam==WTS_SESSION_LOCK || wParam==WTS_CONSOLE_DISCONNECT || wParam==WTS_REMOTE_DISCONNECT)self->Cancel();return 0;
        case WM_DISPLAYCHANGE:case WM_POWERBROADCAST:self->Cancel();return 0;
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{PAINTSTRUCT ps{};BeginPaint(hwnd,&ps);EndPaint(hwnd,&ps);return 0;}
        case WM_CLOSE:self->Cancel();return 0;
        case WM_DESTROY:
            UnregisterHotKey(hwnd,1);self->RemoveTray();WTSUnRegisterSessionNotification(hwnd);PostQuitMessage(0);return 0;
        }
    } catch(const winrt::hresult_error& error) {
        Log(L"Window error: "+std::wstring(error.message()));
        ShowWindow(hwnd,SW_HIDE);
    } catch(...) {Log(L"Unexpected window error");ShowWindow(hwnd,SW_HIDE);}
    return DefWindowProcW(hwnd,message,wParam,lParam);
}
}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int) {
    using namespace reel;
    try {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        const Command command=CommandLine();
        // Capture before COM, icon decode, preset discovery, and GPU startup.
        const CommandPacket initialEvent=CaptureCommand(command,true);
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        LoadLanguage();
        INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
        if(command==Command::Invalid) {MessageBoxW(nullptr,Tr(L"Usage: CopilotReel.exe [--settings | --preview | --background | --diagnostics | --quit]",L"用法：CopilotReel.exe [--settings | --preview | --background | --diagnostics | --quit]"),kAppName,MB_OK|MB_ICONINFORMATION);return 2;}
        const bool smoke=command==Command::SmokeTest;
        const std::wstring suffix=UserSuffix()+(smoke?L".Smoke":L"");
        const std::wstring mutexName=L"Local\\CopilotReel.Instance."+suffix,readyName=L"Local\\CopilotReel.Ready."+suffix;
        HANDLE mutex=CreateMutexW(nullptr,FALSE,mutexName.c_str());
        const bool exists=GetLastError()==ERROR_ALREADY_EXISTS;
        if(!mutex) return 3;
        if(exists) {
            HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,readyName.c_str());
            DWORD readyResult=WAIT_FAILED;
            if(ready) {readyResult=WaitForSingleObject(ready,5000);CloseHandle(ready);}
            if(readyResult!=WAIT_OBJECT_0) {CloseHandle(mutex);return 4;}
            HWND prior=FindWindowW(kWindowClass,smoke?L"Copilot Reel Smoke":kAppName);
            bool forwarded=false;
            if(prior) {
                DWORD pid=0;GetWindowThreadProcessId(prior,&pid);AllowSetForegroundWindow(pid);
                COPYDATASTRUCT copy{kIpcMagic,sizeof(initialEvent),const_cast<CommandPacket*>(&initialEvent)};
                DWORD_PTR result=0;
                forwarded=SendMessageTimeoutW(prior,WM_COPYDATA,0,reinterpret_cast<LPARAM>(&copy),SMTO_ABORTIFHUNG,2000,&result) && result;
            }
            CloseHandle(mutex);return forwarded?0:4;
        }
        HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,readyName.c_str());
        if(ready) ResetEvent(ready);
        if(command==Command::Quit || command==Command::Cancel) {
            if(ready)CloseHandle(ready);CloseHandle(mutex);return 0;
        }
        WNDCLASSEXW cls{sizeof(cls)};cls.lpfnWndProc=WindowProcedure;cls.hInstance=instance;cls.lpszClassName=kWindowClass;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));RegisterClassExW(&cls);
        Controller controller(smoke);
        HWND hwnd=CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,kWindowClass,smoke?L"Copilot Reel Smoke":kAppName,WS_POPUP,0,0,384,384,nullptr,nullptr,instance,&controller);
        if(!hwnd || !controller.Initialize()) {if(hwnd)DestroyWindow(hwnd);if(ready)CloseHandle(ready);CloseHandle(mutex);return 5;}
        if(ready)SetEvent(ready);
        controller.CommandReceived(initialEvent);
        MSG message{};int status;
        while((status=GetMessageW(&message,nullptr,0,0))>0) {
            HWND active=GetActiveWindow();
            if(active && active!=hwnd && IsDialogMessageW(active,&message)) continue;
            TranslateMessage(&message);DispatchMessageW(&message);
        }
        if(ready)CloseHandle(ready);CloseHandle(mutex);
        return status<0?6:static_cast<int>(message.wParam);
    } catch(const winrt::hresult_error& error) {
        Log(L"Startup error: "+std::wstring(error.message()));
        MessageBoxW(nullptr,error.message().c_str(),L"Copilot Reel - startup failed",MB_OK|MB_ICONERROR);return 7;
    } catch(...) {Log(L"Unexpected startup error");return 8;}
}
