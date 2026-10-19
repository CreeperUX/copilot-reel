#include "settings.h"
#include "localization.h"

#include <commdlg.h>
#include <shlobj.h>
#include <algorithm>
#include <array>
#include <cwctype>
#include <map>
#include <memory>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace reel {
namespace {
constexpr wchar_t kSettingsClass[] = L"CopilotReel.Settings";
constexpr std::size_t kMaxApps = 8;
constexpr DWORD kMaxConfigBytes = 512 * 1024;
HWND gSettings = nullptr;

bool IsAbsoluteDataDirectory(std::wstring_view path) {
    const auto separator = [](wchar_t c) { return c == L'\\' || c == L'/'; };
    if (path.size() >= 3 && ((path[0] >= L'A' && path[0] <= L'Z') ||
        (path[0] >= L'a' && path[0] <= L'z')) && path[1] == L':' && separator(path[2])) return true;
    // Accept conventional UNC server/share paths; device namespaces are not data directories.
    if (path.size() < 5 || path[0] != L'\\' || path[1] != L'\\' || path[2] == L'.' || path[2] == L'?') return false;
    const auto share = path.find_first_of(L"\\/", 2);
    return share != std::wstring_view::npos && share > 2 && share + 1 < path.size() && !separator(path[share + 1]);
}

std::wstring DataDirectoryOverride() {
    constexpr wchar_t name[] = L"COPILOT_REEL_DATA_DIR";
    for (int retry = 0; retry < 3; ++retry) {
        SetLastError(ERROR_SUCCESS);
        const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
        if (required == 0) {
            const DWORD code = GetLastError();
            if (code == ERROR_SUCCESS || code == ERROR_ENVVAR_NOT_FOUND) return {};
            throw std::runtime_error("Cannot read COPILOT_REEL_DATA_DIR");
        }
        std::wstring directory(required, L'\0');
        const DWORD copied = GetEnvironmentVariableW(name, directory.data(), required);
        if (copied >= required) continue; // An environment update can race with the length query.
        if (copied == 0) continue;
        directory.resize(copied);
        if (!IsAbsoluteDataDirectory(directory))
            throw std::runtime_error("COPILOT_REEL_DATA_DIR must be an absolute drive or UNC directory");
        return directory;
    }
    throw std::runtime_error("COPILOT_REEL_DATA_DIR changed while being read");
}

std::wstring WinError(DWORD code) {
    wchar_t* message = nullptr;
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
        code, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), reinterpret_cast<wchar_t*>(&message), 0, nullptr);
    std::wstring result = length && message ? std::wstring(message, length) : L"Windows error " + std::to_wstring(code);
    if (message) LocalFree(message);
    while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n')) result.pop_back();
    return result;
}

struct FileHandle {
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~FileHandle() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
    void Close() { if (handle != INVALID_HANDLE_VALUE) { CloseHandle(handle); handle = INVALID_HANDLE_VALUE; } }
};

bool Blank(std::wstring_view text) {
    return text.empty() || std::all_of(text.begin(), text.end(), [](wchar_t c) { return std::iswspace(c) != 0; });
}

bool ValidText(const std::wstring& value, std::size_t limit) {
    return value.size() <= limit && value.find_first_of(std::wstring(L"\0\r\n", 3)) == std::wstring::npos;
}

bool Validate(const std::vector<AppTarget>& targets, std::wstring& error) {
    if (targets.empty() || targets.size() > kMaxApps) {
        error = Tr(L"The reel requires between 1 and 8 applications.", L"轮盘需要 1 至 8 个应用。");
        return false;
    }
    for (std::size_t i = 0; i < targets.size(); ++i) {
        const auto& app = targets[i];
        if (Blank(app.name) || Blank(app.target) || !ValidText(app.name, 128) ||
            !ValidText(app.target, 8192) || !ValidText(app.arguments, 8192) ||
            !ValidText(app.workingDirectory, 8192) ||
            (app.kind != TargetKind::Shell && app.kind != TargetKind::Aumid)) {
            error = std::wstring(Tr(L"Invalid application ", L"无效的应用 ")) + std::to_wstring(i + 1) +
                Tr(L": name and target are required. Names support up to 128 characters; other fields support 8192. Line breaks are not allowed.",
                   L"：名称和目标不能为空。名称最多 128 字符，其他字段最多 8192 字符，不能包含换行。");
            return false;
        }
    }
    return true;
}

std::vector<AppTarget> Defaults() {
    return {
        {L"ChatGPT", TargetKind::Shell, L"https://chatgpt.com", L"", L""},
        {L"Claude", TargetKind::Shell, L"https://claude.ai", L"", L""},
        {L"Gemini", TargetKind::Shell, L"https://gemini.google.com", L"", L""},
        {L"Perplexity", TargetKind::Shell, L"https://www.perplexity.ai", L"", L""},
        {L"Notepad", TargetKind::Shell, L"notepad.exe", L"", L""},
        {L"Windows Settings", TargetKind::Shell, L"ms-settings:", L"", L""}
    };
}

std::wstring Encode(std::wstring_view text) {
    std::wstring encoded;
    encoded.reserve(text.size());
    for (wchar_t c : text) {
        if (c == L'\\') encoded += L"\\\\";
        else if (c == L'\t') encoded += L"\\t";
        else encoded += c;
    }
    return encoded;
}

bool Decode(std::wstring_view text, std::wstring& value) {
    value.clear();
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != L'\\') { value += text[i]; continue; }
        if (++i == text.size()) return false;
        if (text[i] == L'\\') value += L'\\';
        else if (text[i] == L't') value += L'\t';
        else return false;
    }
    return true;
}

bool Parse(std::wstring_view text, std::vector<AppTarget>& targets) {
    using Fields = std::map<std::wstring, std::wstring>;
    std::map<std::wstring, Fields> sections;
    std::wstring section;
    for (std::size_t offset = 0; offset < text.size();) {
        const auto end = text.find(L'\n', offset);
        auto line = text.substr(offset, end == std::wstring_view::npos ? text.size() - offset : end - offset);
        offset = end == std::wstring_view::npos ? text.size() : end + 1;
        if (!line.empty() && line.back() == L'\r') line.remove_suffix(1);
        if (line.empty() || line.front() == L';' || line.front() == L'#') continue;
        if (line.front() == L'[' && line.back() == L']') {
            section = line.substr(1, line.size() - 2);
            if (section.empty() || !sections.emplace(section, Fields{}).second) return false;
            continue;
        }
        const auto equal = line.find(L'=');
        if (section.empty() || equal == std::wstring_view::npos || equal == 0) return false;
        std::wstring value;
        if (!Decode(line.substr(equal + 1), value) ||
            !sections.at(section).emplace(std::wstring(line.substr(0, equal)), std::move(value)).second) return false;
    }
    const auto header = sections.find(L"CopilotReel");
    if (header == sections.end() || header->second.size() != 2) return false;
    const auto version = header->second.find(L"Version");
    const auto countValue = header->second.find(L"Count");
    if (version == header->second.end() || version->second != L"1" || countValue == header->second.end() ||
        countValue->second.size() != 1 || countValue->second[0] < L'1' || countValue->second[0] > L'8') return false;
    const auto count = static_cast<std::size_t>(countValue->second[0] - L'0');
    if (sections.size() != count + 1) return false;
    targets.clear();
    for (std::size_t i = 0; i < count; ++i) {
        const auto found = sections.find(L"App" + std::to_wstring(i));
        if (found == sections.end() || found->second.size() != 5) return false;
        const auto& fields = found->second;
        for (const auto* key : {L"Name", L"Kind", L"Target", L"Arguments", L"WorkingDirectory"})
            if (!fields.contains(key)) return false;
        AppTarget app;
        app.name = fields.at(L"Name");
        const auto& kind = fields.at(L"Kind");
        if (kind == L"shell") app.kind = TargetKind::Shell;
        else if (kind == L"aumid") app.kind = TargetKind::Aumid;
        else return false;
        app.target = fields.at(L"Target");
        app.arguments = fields.at(L"Arguments");
        app.workingDirectory = fields.at(L"WorkingDirectory");
        targets.push_back(std::move(app));
    }
    std::wstring error;
    return Validate(targets, error);
}

enum ControlId : int {
    IdInfo = 100, IdList, IdNameLabel, IdName, IdKindLabel, IdKind, IdTargetLabel,
    IdTarget, IdBrowse, IdArgsLabel, IdArgs, IdWorkingLabel, IdWorking,
    IdAdd, IdUpdate, IdRemove, IdUp, IdDown, IdStatus, IdSave, IdClose, IdLanguageLabel, IdLanguage
};

struct SettingsState {
    HWND notifyOwner = nullptr;
    std::vector<AppTarget> apps;
    std::map<int, HWND> controls;
    HFONT font = nullptr;
    UINT dpi = 96;
    int selected = -1;
    bool populating = false;
    bool draftDirty = false;
    bool dirty = false;
    std::wstring initialError;
    ~SettingsState() { if (font) DeleteObject(font); }
};

HWND Control(SettingsState& state, int id) { return state.controls.at(id); }

std::wstring TextOf(HWND window) {
    const int length = GetWindowTextLengthW(window);
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    const int read = GetWindowTextW(window, text.data(), length + 1);
    text.resize(static_cast<std::size_t>((std::max)(0, read)));
    return text;
}

void Status(SettingsState& state, const std::wstring& text) { SetWindowTextW(Control(state, IdStatus), text.c_str()); }

void RefreshLanguage(HWND window, SettingsState& state) {
    const auto label = [&](int id, const wchar_t* text) {
        SetWindowTextW(Control(state, id), text);
    };
    SetWindowTextW(window, Tr(L"Copilot Reel — Application settings", L"Copilot Reel — 应用设置"));
    label(IdLanguageLabel, Tr(L"Language", L"语言"));
    label(IdInfo, Tr(
        L"Configure 1–8 entries. Default AI entries open websites; browse to use desktop apps.\r\nSave applies your edits. Add creates another entry from the current form.",
        L"配置 1–8 个轮盘入口。默认 AI 入口使用网页，可浏览替换为桌面应用。\r\n编辑后可直接保存；“添加”会把当前表单追加为新应用。"));
    label(IdNameLabel, Tr(L"Display name", L"显示名称"));
    label(IdKindLabel, Tr(L"Launch method", L"启动方式"));
    label(IdTargetLabel, Tr(L"Target: application, shortcut, URI, or AUMID", L"目标：程序、快捷方式、网址、协议或 AUMID"));
    label(IdBrowse, Tr(L"Browse…", L"浏览…"));
    label(IdArgsLabel, Tr(L"Arguments (optional)", L"启动参数（可选）"));
    label(IdWorkingLabel, Tr(L"Working directory (optional; Shell only)", L"工作目录（可选，仅 Shell）"));
    label(IdAdd, Tr(L"Add as new", L"添加为新应用"));
    label(IdUpdate, Tr(L"Update selected", L"更新所选应用"));
    label(IdUp, Tr(L"Up", L"上移"));
    label(IdDown, Tr(L"Down", L"下移"));
    label(IdRemove, Tr(L"Remove", L"移除"));
    label(IdSave, Tr(L"Save and apply", L"保存并应用"));
    label(IdClose, Tr(L"Close", L"关闭"));
    const auto kind = Control(state, IdKind);
    const LRESULT selected = SendMessageW(kind, CB_GETCURSEL, 0, 0);
    SendMessageW(kind, CB_RESETCONTENT, 0, 0);
    SendMessageW(kind, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(L"Shell — application / shortcut / URI", L"Shell — 程序 / 快捷方式 / URI")));
    SendMessageW(kind, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(L"AUMID — installed packaged application", L"AUMID — 已安装的打包应用")));
    SendMessageW(kind, CB_SETCURSEL, selected == 1 ? 1 : 0, 0);
    const auto language = Control(state, IdLanguage);
    SendMessageW(language, CB_RESETCONTENT, 0, 0);
    SendMessageW(language, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
    SendMessageW(language, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(L"Simplified Chinese", L"简体中文")));
    SendMessageW(language, CB_SETCURSEL, CurrentLanguage() == Language::SimplifiedChinese ? 1 : 0, 0);
    Status(state, state.dirty || state.draftDirty ? Tr(L"There are unsaved changes.", L"有未保存的修改。") :
        Tr(L"Save to apply application changes.", L"更改将在保存后应用。"));
}

void UpdateButtons(SettingsState& state) {
    const bool selected = state.selected >= 0 && static_cast<std::size_t>(state.selected) < state.apps.size();
    EnableWindow(Control(state, IdAdd), state.apps.size() < kMaxApps);
    EnableWindow(Control(state, IdUpdate), selected);
    EnableWindow(Control(state, IdRemove), selected && state.apps.size() > 1);
    EnableWindow(Control(state, IdUp), selected && state.selected > 0);
    EnableWindow(Control(state, IdDown), selected && static_cast<std::size_t>(state.selected + 1) < state.apps.size());
    const bool shell = SendMessageW(Control(state, IdKind), CB_GETCURSEL, 0, 0) != 1;
    EnableWindow(Control(state, IdBrowse), shell);
    EnableWindow(Control(state, IdWorking), shell);
}

void Populate(SettingsState& state, int selected) {
    state.populating = true;
    auto list = Control(state, IdList);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const auto& app : state.apps) SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(app.name.c_str()));
    state.selected = (std::clamp)(selected, 0, static_cast<int>(state.apps.size()) - 1);
    SendMessageW(list, LB_SETCURSEL, state.selected, 0);
    const auto& app = state.apps.at(static_cast<std::size_t>(state.selected));
    SetWindowTextW(Control(state, IdName), app.name.c_str());
    SendMessageW(Control(state, IdKind), CB_SETCURSEL, app.kind == TargetKind::Aumid ? 1 : 0, 0);
    SetWindowTextW(Control(state, IdTarget), app.target.c_str());
    SetWindowTextW(Control(state, IdArgs), app.arguments.c_str());
    SetWindowTextW(Control(state, IdWorking), app.workingDirectory.c_str());
    state.draftDirty = false;
    state.populating = false;
    UpdateButtons(state);
}

bool ReadDraft(HWND window, SettingsState& state, AppTarget& app) {
    app.name = TextOf(Control(state, IdName));
    app.kind = SendMessageW(Control(state, IdKind), CB_GETCURSEL, 0, 0) == 1 ? TargetKind::Aumid : TargetKind::Shell;
    app.target = TextOf(Control(state, IdTarget));
    app.arguments = TextOf(Control(state, IdArgs));
    app.workingDirectory = app.kind == TargetKind::Shell ? TextOf(Control(state, IdWorking)) : L"";
    std::wstring error;
    if (!Validate({app}, error)) {
        MessageBoxW(window, error.c_str(), Tr(L"Cannot apply changes", L"无法应用修改"), MB_OK | MB_ICONWARNING);
        return false;
    }
    return true;
}

bool ApplyDraft(HWND window, SettingsState& state) {
    if (!state.draftDirty || state.selected < 0) return true;
    AppTarget app;
    if (!ReadDraft(window, state, app)) return false;
    state.apps.at(static_cast<std::size_t>(state.selected)) = std::move(app);
    state.draftDirty = false;
    state.dirty = true;
    return true;
}

void SetFont(HWND window, SettingsState& state) {
    state.dpi = GetDpiForWindow(window);
    auto font = CreateFontW(-MulDiv(10, static_cast<int>(state.dpi), 72), 0, 0, 0,
        FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    if (!font) return;
    for (const auto& [id, control] : state.controls) {
        (void)id;
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    if (state.font) DeleteObject(state.font);
    state.font = font;
}

void Layout(HWND window, SettingsState& state) {
    RECT client{};
    GetClientRect(window, &client);
    const auto scale = [&state](int value) { return MulDiv(value, static_cast<int>(state.dpi), 96); };
    const int width = MulDiv(client.right, 96, static_cast<int>(state.dpi));
    const int height = MulDiv(client.bottom, 96, static_cast<int>(state.dpi));
    const auto place = [&](int id, int x, int y, int w, int h) {
        MoveWindow(Control(state, id), scale(x), scale(y), scale((std::max)(1, w)), scale(h), TRUE);
    };
    const int right = 250;
    const int fieldWidth = width - right - 24;
    place(IdLanguageLabel, 20, 20, 84, 22);
    place(IdLanguage, 110, 16, 210, 160);
    place(IdInfo, 20, 60, width - 40, 52);
    place(IdList, 20, 124, 210, height - 248);
    place(IdNameLabel, right, 124, fieldWidth, 22);
    place(IdName, right, 149, fieldWidth, 28);
    place(IdKindLabel, right, 189, fieldWidth, 22);
    place(IdKind, right, 214, fieldWidth, 160);
    place(IdTargetLabel, right, 256, fieldWidth, 22);
    place(IdTarget, right, 281, fieldWidth - 88, 28);
    place(IdBrowse, width - 104, 281, 80, 28);
    place(IdArgsLabel, right, 322, fieldWidth, 22);
    place(IdArgs, right, 347, fieldWidth, 28);
    place(IdWorkingLabel, right, 388, fieldWidth, 22);
    place(IdWorking, right, 413, fieldWidth, 28);
    place(IdAdd, right, 459, 114, 32);
    place(IdUpdate, right + 126, 459, 140, 32);
    place(IdUp, 20, height - 112, 62, 30);
    place(IdDown, 92, height - 112, 62, 30);
    place(IdRemove, 164, height - 112, 66, 30);
    place(IdStatus, 20, height - 71, width - 300, 48);
    place(IdSave, width - 264, height - 58, 144, 34);
    place(IdClose, width - 108, height - 58, 84, 34);
}

bool BuildControls(HWND window, SettingsState& state) {
    const auto make = [&](int id, const wchar_t* className, const wchar_t* text, DWORD style, DWORD extended = 0) {
        HWND control = CreateWindowExW(extended, className, text, WS_CHILD | WS_VISIBLE | style,
            0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr), nullptr);
        if (control) state.controls.emplace(id, control);
        return control != nullptr;
    };
    if (!make(IdLanguageLabel, L"STATIC", L"", 0) ||
        !make(IdLanguage, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL) ||
        !make(IdInfo, L"STATIC", L"", 0) ||
        !make(IdList, L"LISTBOX", L"", LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE) ||
        !make(IdNameLabel, L"STATIC", L"", 0) ||
        !make(IdName, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE) ||
        !make(IdKindLabel, L"STATIC", L"", 0) ||
        !make(IdKind, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL) ||
        !make(IdTargetLabel, L"STATIC", L"", 0) ||
        !make(IdTarget, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE) ||
        !make(IdBrowse, L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP) ||
        !make(IdArgsLabel, L"STATIC", L"", 0) ||
        !make(IdArgs, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE) ||
        !make(IdWorkingLabel, L"STATIC", L"", 0) ||
        !make(IdWorking, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE) ||
        !make(IdAdd, L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP) ||
        !make(IdUpdate, L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP) ||
        !make(IdUp, L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP) ||
        !make(IdDown, L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP) ||
        !make(IdRemove, L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP) ||
        !make(IdStatus, L"STATIC", L"", 0) ||
        !make(IdSave, L"BUTTON", L"", BS_DEFPUSHBUTTON | WS_TABSTOP) ||
        !make(IdClose, L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP)) return false;
    RefreshLanguage(window, state);
    for (int id : {IdName, IdTarget, IdArgs, IdWorking})
        SendMessageW(Control(state, id), EM_SETLIMITTEXT, id == IdName ? 128 : 8192, 0);
    SetFont(window, state);
    Populate(state, 0);
    return true;
}

void Browse(HWND window, SettingsState& state) {
    std::array<wchar_t, 32768> path{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = Tr(L"Applications or shortcuts (*.exe;*.lnk)\0*.exe;*.lnk\0All files (*.*)\0*.*\0\0",
        L"应用或快捷方式 (*.exe;*.lnk)\0*.exe;*.lnk\0所有文件 (*.*)\0*.*\0\0");
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrTitle = Tr(L"Select an application for the reel", L"选择轮盘要打开的应用");
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT | OFN_NODEREFERENCELINKS;
    if (GetOpenFileNameW(&dialog)) {
        SetWindowTextW(Control(state, IdTarget), path.data());
        SendMessageW(Control(state, IdKind), CB_SETCURSEL, 0, 0);
        state.draftDirty = true;
        UpdateButtons(state);
    } else if (const DWORD error = CommDlgExtendedError(); error != 0) {
        Status(state, std::wstring(Tr(L"File picker error: ", L"文件选择器错误：")) + std::to_wstring(error));
    }
}

void SaveFromWindow(HWND window, SettingsState& state) {
    if (!ApplyDraft(window, state)) return;
    Populate(state, state.selected);
    std::wstring error;
    if (!SaveTargets(state.apps, error)) {
        MessageBoxW(window, error.c_str(), Tr(L"Save failed; original configuration retained", L"保存失败；原配置未替换"), MB_OK | MB_ICONERROR);
        return;
    }
    state.dirty = false;
    Status(state, Tr(L"Saved. The reel configuration is now applied.", L"已保存，轮盘配置已应用。"));
    if (IsWindow(state.notifyOwner)) PostMessageW(state.notifyOwner, kConfigChangedMessage, 0, 0);
}

LRESULT HandleSettings(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<SettingsState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = static_cast<std::unique_ptr<SettingsState>*>(create->lpCreateParams)->release();
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (!state) return DefWindowProcW(window, message, wParam, lParam);
    switch (message) {
    case WM_CREATE:
        if (!BuildControls(window, *state)) return -1;
        if (!state->initialError.empty()) {
            Status(*state, Tr(L"Defaults loaded. Check the original configuration before saving.", L"配置读取失败；显示默认入口。保存前请检查原文件。"));
            PostMessageW(window, WM_APP + 100, 0, 0);
        }
        return 0;
    case WM_APP + 100:
        MessageBoxW(window, state->initialError.c_str(), Tr(L"Configuration notice", L"配置读取提示"), MB_OK | MB_ICONWARNING);
        return 0;
    case WM_SIZE:
        if (state->controls.size() == 23) Layout(window, *state);
        return 0;
    case WM_GETMINMAXINFO: {
        const UINT dpi = GetDpiForWindow(window);
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize = {MulDiv(790, static_cast<int>(dpi), 96), MulDiv(655, static_cast<int>(dpi), 96)};
        return 0;
    }
    case WM_DPICHANGED: {
        auto* rect = reinterpret_cast<RECT*>(lParam);
        SetFont(window, *state);
        SetWindowPos(window, nullptr, rect->left, rect->top, rect->right - rect->left,
            rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        Layout(window, *state);
        return 0;
    }
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        SetFont(window, *state);
        InvalidateRect(window, nullptr, TRUE);
        return 0;
    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        const int notification = HIWORD(wParam);
        if (state->populating) return 0;
        if (id == IdLanguage && notification == CBN_SELCHANGE) {
            const auto selected = SendMessageW(Control(*state, IdLanguage), CB_GETCURSEL, 0, 0);
            const auto language = selected == 1 ? Language::SimplifiedChinese : Language::English;
            SetLanguage(language);
            const bool saved = SaveLanguage(language);
            RefreshLanguage(window, *state);
            if (!saved) Status(*state, Tr(L"Language changed for this session; the preference could not be saved.", L"当前会话语言已更改，但无法保存语言偏好。"));
            if (IsWindow(state->notifyOwner)) PostMessageW(state->notifyOwner, kLanguageChangedMessage, 0, 0);
            return 0;
        }
        if (((id == IdName || id == IdTarget || id == IdArgs || id == IdWorking) && notification == EN_CHANGE) ||
            (id == IdKind && notification == CBN_SELCHANGE)) {
            state->draftDirty = true;
            Status(*state, Tr(L"There are unsaved changes.", L"有未保存的修改。"));
            UpdateButtons(*state);
            return 0;
        }
        if (id == IdList && notification == LBN_SELCHANGE) {
            const int next = static_cast<int>(SendMessageW(Control(*state, IdList), LB_GETCURSEL, 0, 0));
            if (next < 0) return 0;
            if (!ApplyDraft(window, *state)) SendMessageW(Control(*state, IdList), LB_SETCURSEL, state->selected, 0);
            else Populate(*state, next);
            return 0;
        }
        if (notification != BN_CLICKED) return 0;
        switch (id) {
        case IdBrowse: Browse(window, *state); break;
        case IdAdd: {
            if (state->apps.size() >= kMaxApps) break;
            AppTarget app;
            if (!ReadDraft(window, *state, app)) break;
            state->apps.push_back(std::move(app));
            state->dirty = true;
            Populate(*state, static_cast<int>(state->apps.size()) - 1);
            Status(*state, Tr(L"Entry added. Save to apply.", L"已加入列表；请保存以应用。"));
            break;
        }
        case IdUpdate:
            state->draftDirty = true;
            if (ApplyDraft(window, *state)) {
                Populate(*state, state->selected);
                Status(*state, Tr(L"Entry updated. Save to apply.", L"已更新列表；请保存以应用。"));
            }
            break;
        case IdRemove:
            if (state->apps.size() > 1 && state->selected >= 0) {
                state->apps.erase(state->apps.begin() + state->selected);
                state->dirty = true;
                Populate(*state, state->selected);
                Status(*state, Tr(L"Entry removed. Save to apply.", L"已从列表移除；请保存以应用。"));
            }
            break;
        case IdUp:
        case IdDown: {
            const int next = state->selected + (id == IdUp ? -1 : 1);
            if (next < 0 || next >= static_cast<int>(state->apps.size()) || !ApplyDraft(window, *state)) break;
            std::swap(state->apps.at(static_cast<std::size_t>(state->selected)), state->apps.at(static_cast<std::size_t>(next)));
            state->dirty = true;
            Populate(*state, next);
            Status(*state, Tr(L"Order updated. Save to apply.", L"已调整顺序；请保存以应用。"));
            break;
        }
        case IDOK:
        case IdSave: SaveFromWindow(window, *state); break;
        case IDCANCEL:
        case IdClose: SendMessageW(window, WM_CLOSE, 0, 0); break;
        default: break;
        }
        return 0;
    }
    case WM_CLOSE:
        if (state->dirty || state->draftDirty) {
            const int answer = MessageBoxW(window, Tr(L"Save the reel application configuration?", L"是否保存轮盘应用配置？"),
                Tr(L"Unsaved changes", L"存在未保存的修改"), MB_YESNOCANCEL | MB_ICONQUESTION);
            if (answer == IDCANCEL) return 0;
            if (answer == IDYES) {
                SaveFromWindow(window, *state);
                if (state->dirty || state->draftDirty) return 0;
            }
        }
        DestroyWindow(window);
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        if (gSettings == window) gSettings = nullptr;
        delete state;
        return DefWindowProcW(window, message, wParam, lParam);
    default: return DefWindowProcW(window, message, wParam, lParam);
    }
}

LRESULT CALLBACK SettingsProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept {
    try { return HandleSettings(window, message, wParam, lParam); }
    catch (...) {
        if (message == WM_NCCREATE) return FALSE;
        if (message == WM_CREATE) return -1;
        MessageBoxW(window, Tr(L"The operation could not be completed. Close settings and try again. The original configuration will not be partially overwritten.",
            L"无法完成此操作。请关闭设置窗口后重试。原配置文件不会被部分写入。"), L"Copilot Reel", MB_OK | MB_ICONERROR);
        return 0;
    }
}
} // namespace

std::wstring ConfigPath() {
    if (auto directory = DataDirectoryOverride(); !directory.empty()) {
        if (directory.back() != L'\\' && directory.back() != L'/') directory += L'\\';
        return directory + L"apps.ini";
    }
    PWSTR directory = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &directory))) return {};
    std::wstring path;
    try { path = directory; } catch (...) { CoTaskMemFree(directory); throw; }
    CoTaskMemFree(directory);
    return path + L"\\CopilotReel\\apps.ini";
}

std::vector<AppTarget> LoadTargets(std::wstring& error) {
    error.clear();
    try {
        const auto path = ConfigPath();
        if (path.empty()) { error = Tr(L"Cannot locate the user configuration directory.", L"无法定位用户配置目录。"); return Defaults(); }
        FileHandle file;
        file.handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file.handle == INVALID_HANDLE_VALUE) {
            const DWORD code = GetLastError();
            if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND) {
                auto defaults = Defaults();
                SaveTargets(defaults, error);
                return defaults;
            }
            error = std::wstring(Tr(L"Cannot read configuration: ", L"无法读取配置：")) + WinError(code);
            return Defaults();
        }
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file.handle, &size) || size.QuadPart < 2 || size.QuadPart > kMaxConfigBytes || (size.QuadPart % 2) != 0) {
            error = Tr(L"Invalid configuration length. The original file is retained; default entries are in use.", L"配置文件长度无效。原文件已保留，暂时使用默认入口。");
            return Defaults();
        }
        std::wstring content(static_cast<std::size_t>(size.QuadPart / 2), L'\0');
        DWORD read = 0;
        if (!ReadFile(file.handle, content.data(), static_cast<DWORD>(size.QuadPart), &read, nullptr) || read != size.QuadPart) {
            error = Tr(L"Configuration read failed. The original file is retained; default entries are in use.", L"配置文件读取失败。原文件已保留，暂时使用默认入口。");
            return Defaults();
        }
        std::vector<AppTarget> targets;
        if (content.front() != L'\xfeff' || !Parse(std::wstring_view(content).substr(1), targets)) {
            error = std::wstring(Tr(L"Invalid configuration format. Expected UTF-16 LE with a BOM, Version=1, and 1–8 complete entries. The original file is retained; default entries are in use.\r\n\r\n",
                L"配置文件格式无效。需要带 BOM 的 UTF-16 LE、Version=1 和 1–8 个完整入口。原文件已保留，暂时使用默认入口。\r\n\r\n")) + path;
            return Defaults();
        }
        return targets;
    } catch (...) {
        error = Tr(L"An error occurred while reading configuration. The original file is retained; default entries are in use.", L"读取配置时发生错误。原文件已保留，暂时使用默认入口。");
        return Defaults();
    }
}

bool SaveTargets(const std::vector<AppTarget>& targets, std::wstring& error) {
    error.clear();
    std::wstring temporary;
    bool temporaryOwned = false;
    try {
        if (!Validate(targets, error)) return false;
        const auto path = ConfigPath();
        if (path.empty()) { error = Tr(L"Cannot locate the user configuration directory.", L"无法定位用户配置目录。"); return false; }
        const auto directory = path.substr(0, path.find_last_of(L'\\'));
        if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
            error = std::wstring(Tr(L"Cannot create the configuration directory: ", L"无法创建配置目录：")) + WinError(GetLastError());
            return false;
        }
        std::wstring content = L"\xfeff; Copilot Reel configuration. UTF-16 LE; backslashes are escaped.\r\n[CopilotReel]\r\nVersion=1\r\nCount=" + std::to_wstring(targets.size()) + L"\r\n";
        for (std::size_t i = 0; i < targets.size(); ++i) {
            const auto& app = targets[i];
            content += L"\r\n[App" + std::to_wstring(i) + L"]\r\nName=" + Encode(app.name) +
                L"\r\nKind=" + (app.kind == TargetKind::Shell ? std::wstring(L"shell") : std::wstring(L"aumid")) +
                L"\r\nTarget=" + Encode(app.target) + L"\r\nArguments=" + Encode(app.arguments) +
                L"\r\nWorkingDirectory=" + Encode(app.workingDirectory) + L"\r\n";
        }
        if (content.size() * sizeof(wchar_t) > kMaxConfigBytes) {
            error = Tr(L"The configuration exceeds 512 KiB. Shorten paths or arguments.", L"配置总大小超过 512 KiB，请缩短参数或路径。");
            return false;
        }
        FileHandle file;
        for (unsigned int attempt = 0; attempt < 16; ++attempt) {
            temporary = path + L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                std::to_wstring(GetCurrentThreadId()) + L"-" + std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(attempt);
            file.handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
            if (file.handle != INVALID_HANDLE_VALUE) { temporaryOwned = true; break; }
            if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) break;
        }
        if (file.handle == INVALID_HANDLE_VALUE) {
            error = std::wstring(Tr(L"Cannot create a temporary configuration file: ", L"无法创建临时配置文件：")) + WinError(GetLastError());
            temporary.clear(); // Never delete a file that this call did not create.
            return false;
        }
        const DWORD bytes = static_cast<DWORD>(content.size() * sizeof(wchar_t));
        DWORD written = 0;
        const bool writtenOk = WriteFile(file.handle, content.data(), bytes, &written, nullptr) && written == bytes;
        const bool flushed = writtenOk && FlushFileBuffers(file.handle);
        const DWORD code = GetLastError();
        file.Close();
        if (!flushed) {
            error = std::wstring(Tr(L"Configuration write failed; the original file is unchanged: ", L"配置写入失败，原文件保持不变：")) + WinError(code);
            DeleteFileW(temporary.c_str());
            return false;
        }
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            error = std::wstring(Tr(L"Cannot replace the configuration; the original file is unchanged: ", L"无法替换配置文件，原文件保持不变：")) + WinError(GetLastError());
            DeleteFileW(temporary.c_str());
            return false;
        }
        return true;
    } catch (...) {
        if (temporaryOwned && !temporary.empty()) DeleteFileW(temporary.c_str());
        error = Tr(L"An error occurred while saving configuration; the original file is unchanged.", L"保存配置时发生错误，原文件保持不变。");
        return false;
    }
}

void ShowSettings(HWND owner) {
    try {
        if (gSettings && IsWindow(gSettings)) {
            if (auto* state = reinterpret_cast<SettingsState*>(GetWindowLongPtrW(gSettings, GWLP_USERDATA))) state->notifyOwner = owner;
            ShowWindow(gSettings, SW_RESTORE);
            SetForegroundWindow(gSettings);
            return;
        }
        WNDCLASSEXW windowClass{sizeof(windowClass)};
        windowClass.lpfnWndProc = SettingsProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        windowClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
        windowClass.lpszClassName = kSettingsClass;
        if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            MessageBoxW(owner, Tr(L"Cannot register the settings window.", L"无法注册设置窗口。"), kAppName, MB_OK | MB_ICONERROR);
            return;
        }
        auto state = std::make_unique<SettingsState>();
        state->notifyOwner = owner;
        state->apps = LoadTargets(state->initialError);
        // A visible ownerless window remains usable when the controller / reel is hidden.
        // Ownership of state transfers in WM_NCCREATE; WM_NCDESTROY releases it.
        const UINT dpi = GetDpiForSystem();
        gSettings = CreateWindowExW(WS_EX_CONTROLPARENT, kSettingsClass, Tr(L"Copilot Reel — Application settings", L"Copilot Reel — 应用设置"),
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            MulDiv(850, static_cast<int>(dpi), 96), MulDiv(685, static_cast<int>(dpi), 96),
            nullptr, nullptr, GetModuleHandleW(nullptr), &state);
        if (!gSettings) {
            MessageBoxW(owner, Tr(L"Cannot create the settings window.", L"无法创建设置窗口。"), kAppName, MB_OK | MB_ICONERROR);
            return;
        }
        ShowWindow(gSettings, SW_SHOWNORMAL);
        UpdateWindow(gSettings);
        SetForegroundWindow(gSettings);
    } catch (...) {
        MessageBoxW(owner, Tr(L"Cannot open application settings.", L"无法打开应用设置。"), kAppName, MB_OK | MB_ICONERROR);
    }
}
} // namespace reel
