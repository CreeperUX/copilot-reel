#include "app_types.h"
#include "appearance.h"
#include "core.h"
#include "renderer.h"
#include "settings.h"
#include "localization.h"
#include "osd.h"
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <propsys.h>
#include <sddl.h>
#include <wtsapi32.h>
#include <commctrl.h>
#include <winrt/base.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

namespace reel {
namespace {
constexpr UINT kSmokeTimer=101, kIdleTimer=102;
constexpr UINT kCommandPreview=201,kCommandSettings=202,kCommandDiagnostics=203,kCommandExit=204;
constexpr ULONG_PTR kIpcMagic=0x5245454c;
constexpr PROPERTYKEY kFastPathKey={{0x38652bca,0x4329,0x4e74,{0x86,0xf9,0x39,0xcf,0x29,0x34,0x5e,0xea}},2};

std::wstring DataDirectory() {
    wchar_t overridePath[32768]{};
    const DWORD overrideLength=GetEnvironmentVariableW(L"COPILOT_REEL_DATA_DIR",overridePath,32768);
    if(overrideLength>0 && overrideLength<32768 && std::filesystem::path(overridePath).is_absolute()) {
        std::filesystem::create_directories(overridePath);return overridePath;
    }
    PWSTR path=nullptr;
    std::wstring result;
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&path))) { result=path; CoTaskMemFree(path); }
    else { wchar_t temp[MAX_PATH]{}; GetTempPathW(MAX_PATH,temp); result=temp; }
    result+=L"\\CopilotReel"; CreateDirectoryW(result.c_str(),nullptr); return result;
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
        MessageBoxW(nullptr,(std::wstring(Tr(L"Could not open: ",L"无法打开："))+target.name+Tr(L"\nCheck the target and arguments in App settings.\n\n",L"\n请检查应用设置中的目标与参数。\n\n")+number).c_str(),kAppName,MB_OK|MB_ICONERROR);
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
        appearance_=std::make_unique<AppearanceSource>(window);
        renderer_=std::make_unique<OrbitRenderer>(window);
        std::wstring error;
        if(!renderer_->Initialize(error)) {
            Log(L"Renderer initialization failed: "+error);
            MessageBoxW(nullptr,(std::wstring(Tr(L"Could not initialize native Windows rendering.\n",L"无法初始化 Windows 原生绘制。\n"))+error).c_str(),kAppName,MB_OK|MB_ICONERROR); return false;
        }
        RegisterFastPath(window);
        WTSRegisterSessionNotification(window,NOTIFY_FOR_THIS_SESSION);
        RefreshAppearance(); Reload(); AddTray();
        // This ordinary optional shortcut opens click-to-select preview; it never intercepts Copilot.
        hotkey_=RegisterHotKey(window,1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,'R')!=FALSE;
        ready_=true;
        Log(L"Ready; fast-path HWND registered; Ctrl+Alt+R "+std::wstring(hotkey_?L"available":L"unavailable"));
        return true;
    }
    void CommandReceived(Command command) {
        switch(command) {
        case Command::Background: break;
        case Command::Settings: Cancel();ShowOsd(window);break;
        case Command::Preview: case Command::Tap: Start(true);break;
        case Command::HoldStart: Start(false);break;
        case Command::HoldStop: if(!preview_) Release();break;
        case Command::Cancel: Cancel();break;
        case Command::Quit: if(CloseOsd()) DestroyWindow(window);break;
        case Command::Diagnostics: SaveDiagnostics();break;
        case Command::SmokeTest: BeginSmoke();break;
        default: break;
        }
    }
    void Start(bool preview) {
        if(gesture_.Active()) return;
        preview_=preview;
        GetCursorPos(&anchor_);
        const HMONITOR monitor=MonitorFromPoint(anchor_,MONITOR_DEFAULTTONEAREST);
        UINT xDpi=96,yDpi=96;
        // GetDpiForMonitor supplies the selected display DPI for the initial placement.
        using GetDpiForMonitorFn=HRESULT(WINAPI*)(HMONITOR,int,UINT*,UINT*);
        HMODULE shcore=LoadLibraryExW(L"shcore.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(shcore) {
            auto getDpi=reinterpret_cast<GetDpiForMonitorFn>(GetProcAddress(shcore,"GetDpiForMonitor"));
            if(getDpi) getDpi(monitor,0,&xDpi,&yDpi);
            FreeLibrary(shcore);
        }
        dpi_=xDpi;
        const int size=MulDiv(static_cast<int>(kCanvasDip),dpi_,96);
        MONITORINFO info{sizeof(info)};
        if(!GetMonitorInfoW(monitor,&info)) return;
        const auto& work=info.rcWork;
        const auto placement=PlaceWheel(anchor_.x,anchor_.y,{work.left,work.top,work.right,work.bottom},size,kDeadZoneDip*dpi_/96.0);
        if(!placement.fits) {
            Log(L"Wheel footprint exceeds monitor work area; opening compact controls instead.");
            ShowOsd(window);return;
        }
        centerX_=placement.centerX;centerY_=placement.centerY;
        SetWindowPos(window,HWND_TOPMOST,placement.left,placement.top,size,size,SWP_NOACTIVATE);
        gesture_.Start(placement.awaitingCenter);
        renderer_->Resize(dpi_);renderer_->SetAppearance(currentAppearance_);renderer_->SetSelection(-1);
        renderer_->SetAwaitCenter(placement.awaitingCenter);
        ShowWindow(window,SW_SHOWNOACTIVATE);renderer_->Show();
        RegisterRawInput(true);
        // Only a one-shot abandonment timeout; animations and mouse tracking have no polling timer.
        SetTimer(window,kIdleTimer,preview?15000:60000,nullptr);
        RECT bounds{};GetWindowRect(window,&bounds);
        Log(std::wstring(preview?L"Preview opened":L"Hold started")+L"; pointer="+std::to_wstring(anchor_.x)+L","+std::to_wstring(anchor_.y)+L"; window="+std::to_wstring(bounds.left)+L","+std::to_wstring(bounds.top)+L","+std::to_wstring(bounds.right)+L","+std::to_wstring(bounds.bottom)+L"; awaitingCenter="+(gesture_.AwaitingCenter()?L"true":L"false"));
    }
    void MovePointer() {
        if(!gesture_.Active()) return;
        POINT p{};GetCursorPos(&p);
        const bool wasAwaiting=gesture_.AwaitingCenter();
        if(gesture_.ObservePointer((p.x-centerX_)*96.0/dpi_,(p.y-centerY_)*96.0/dpi_,static_cast<unsigned>(items_.size()))) {
            renderer_->SetAwaitCenter(gesture_.AwaitingCenter());
            renderer_->SetSelection(gesture_.Selected());
            if(wasAwaiting && !gesture_.AwaitingCenter()) Log(L"Center entered; direction selection armed");
        }
    }
    void Release() {
        if(!gesture_.Active()) return;
        MovePointer();
        const auto selected=gesture_.Release();
        Hide();
        if(selected && static_cast<size_t>(*selected)<items_.size()) {
            if(smoke) Log(L"Smoke simulated launch: "+items_[*selected].name);
            else Launch(items_[*selected],window);
        }
        Log(selected?L"Gesture committed":L"Gesture canceled at center");
    }
    void Cancel() {
        if(!gesture_.Active()) return;
        gesture_.Cancel();Hide();Log(L"Canceled");
    }
    void Hide() {
        KillTimer(window,kIdleTimer);
        RegisterRawInput(false);
        renderer_->Hide();ShowWindow(window,SW_HIDE);
        preview_=false;
    }
    void RefreshAppearance() {
        MSG ignored{};
        while(PeekMessageW(&ignored,window,kAppearanceMessage,kAppearanceMessage,PM_REMOVE)) {}
        currentAppearance_=appearance_->Read();
        renderer_->SetAppearance(currentAppearance_);
        RefreshOsdAppearance();
        Log(L"Appearance: "+std::wstring(currentAppearance_.dark?L"dark":L"light")+L", transparency="+(currentAppearance_.transparency?L"on":L"off"));
    }
    void Reload() {
        Cancel();
        std::wstring error;items_=LoadTargets(error);
        renderer_->SetItems(items_);
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
        else if(raw->header.dwType==RIM_TYPEKEYBOARD && raw->data.keyboard.VKey==VK_ESCAPE && !(raw->data.keyboard.Flags&RI_KEY_BREAK)) Cancel();
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
        text<<L"Copilot Reel 0.2.0\r\n"<<L"DPI="<<dpi_<<L"\r\n"<<L"dark="<<currentAppearance_.dark<<L"\r\ntransparencyRequested="<<currentAppearance_.transparency
            <<L"\r\nanimations="<<currentAppearance_.animations<<L"\r\nhighContrast="<<currentAppearance_.highContrast<<L"\r\ntargetCount="<<items_.size()<<L"\r\n"<<renderer_->Diagnostics();
        const auto bytes=winrt::to_string(text.str());
        const auto path=DataDirectory()+L"\\diagnostics.txt";
        HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file!=INVALID_HANDLE_VALUE) {DWORD written=0;WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr);CloseHandle(file);}
        Log(L"Diagnostics written");
    }
    void BeginSmoke() {
        smokeStep_=0;Start(true);SetTimer(window,kSmokeTimer,1000,nullptr);
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
    void DpiChanged(UINT dpi) {dpi_=dpi;if(renderer_)renderer_->Resize(dpi_);}
private:
    std::unique_ptr<AppearanceSource> appearance_;
    std::unique_ptr<OrbitRenderer> renderer_;
    Appearance currentAppearance_;
    std::vector<AppTarget> items_;
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
            if(!copy || copy->dwData!=kIpcMagic || copy->cbData!=sizeof(unsigned) || !copy->lpData) return FALSE;
            unsigned command=0;memcpy(&command,copy->lpData,sizeof(command));
            if(command>=static_cast<unsigned>(Command::Invalid)) return FALSE;
            // Queue after acknowledging the client. No rendering or Shell activation inside SendMessage.
            PostMessageW(hwnd,WM_APP+20,command,0);return TRUE;
        }
        case WM_APP+20:self->CommandReceived(static_cast<Command>(wParam));return 0;
        case kAppearanceMessage:case WM_SETTINGCHANGE:case WM_THEMECHANGED:case WM_SYSCOLORCHANGE:self->RefreshAppearance();return 0;
        case kConfigChangedMessage:self->Reload();return 0;
        case kLanguageChangedMessage:self->RefreshAppearance();self->RemoveTray();self->AddTray();return 0;
        case WM_INPUT:self->RawInput(reinterpret_cast<HRAWINPUT>(lParam));break;
        case WM_MOUSEMOVE:self->MovePointer();return 0;
        case WM_LBUTTONUP:if(self->IsPreview())self->Release();return 0;
        case WM_RBUTTONUP:self->Cancel();return 0;
        case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
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
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        LoadLanguage();
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
        const Command command=CommandLine();
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
                const unsigned value=static_cast<unsigned>(command);
                COPYDATASTRUCT copy{kIpcMagic,sizeof(value),const_cast<unsigned*>(&value)};
                DWORD_PTR result=0;
                forwarded=SendMessageTimeoutW(prior,WM_COPYDATA,0,reinterpret_cast<LPARAM>(&copy),SMTO_ABORTIFHUNG,2000,&result) && result;
            }
            CloseHandle(mutex);return forwarded?0:4;
        }
        HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,readyName.c_str());
        if(ready) ResetEvent(ready);
        if(command==Command::Quit || command==Command::Cancel || command==Command::HoldStop) {
            if(ready)CloseHandle(ready);CloseHandle(mutex);return 0;
        }
        WNDCLASSEXW cls{sizeof(cls)};cls.lpfnWndProc=WindowProcedure;cls.hInstance=instance;cls.lpszClassName=kWindowClass;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));RegisterClassExW(&cls);
        Controller controller(smoke);
        HWND hwnd=CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,kWindowClass,smoke?L"Copilot Reel Smoke":kAppName,WS_POPUP,0,0,384,384,nullptr,nullptr,instance,&controller);
        if(!hwnd || !controller.Initialize()) {if(hwnd)DestroyWindow(hwnd);if(ready)CloseHandle(ready);CloseHandle(mutex);return 5;}
        if(ready)SetEvent(ready);
        if(command==Command::HoldStart) {
            // URI cold activation has no gesture sequence ID. First cold hold only warms the receiver.
            Log(L"Cold URI hold: resident initialized; first gesture suppressed to avoid stale release races");
        } else controller.CommandReceived(command);
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
