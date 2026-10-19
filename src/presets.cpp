#include "presets.h"

#include <appmodel.h>
#include <shlobj.h>
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <string_view>

namespace reel {
namespace {
std::wstring Lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return value;
}

std::wstring Folder(REFKNOWNFOLDERID id) {
    PWSTR value = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &value))) return {};
    std::wstring result(value);
    CoTaskMemFree(value);
    return result;
}

bool HasPackage(const wchar_t* family) {
    UINT32 count = 0, length = 0;
    const LONG status = GetPackagesByPackageFamily(family, &count, nullptr, &length, nullptr);
    return status == ERROR_INSUFFICIENT_BUFFER && count != 0;
}

std::vector<std::filesystem::path> Shortcuts() {
    std::vector<std::filesystem::path> result;
    for (const auto id : {&FOLDERID_Programs, &FOLDERID_CommonPrograms}) {
        const auto root = Folder(*id);
        if (root.empty()) continue;
        std::error_code error;
        std::filesystem::recursive_directory_iterator iterator(root,
            std::filesystem::directory_options::skip_permission_denied, error), end;
        for (; iterator != end && !error; iterator.increment(error)) {
            if (Lower(iterator->path().extension().wstring()) == L".lnk")
                result.push_back(iterator->path());
        }
    }
    return result;
}

std::wstring FindShortcut(const std::vector<std::filesystem::path>& shortcuts,
                          std::initializer_list<std::wstring_view> names) {
    for (const auto& shortcut : shortcuts) {
        const auto stem = Lower(shortcut.stem().wstring());
        for (const auto name : names) if (stem == name) return shortcut.wstring();
    }
    return {};
}

AppTarget InstalledTarget(const wchar_t* name,
                             const std::vector<std::filesystem::path>& shortcuts,
                             std::initializer_list<std::wstring_view> names,
                             const wchar_t* relativePath = nullptr) {
    auto path = FindShortcut(shortcuts, names);
    if (path.empty() && relativePath) {
        const auto local = Folder(FOLDERID_LocalAppData);
        if (!local.empty()) {
            const auto candidate = std::filesystem::path(local) / relativePath;
            std::error_code error;
            if (std::filesystem::is_regular_file(candidate, error)) path = candidate.wstring();
        }
    }
    return {name, TargetKind::Shell, path, {}, {}};
}
} // namespace

std::vector<AppTarget> AgentPresets() {
    const auto shortcuts = Shortcuts();
    std::vector<AppTarget> result;
    // The installed package has retained the OpenAI.Codex identity, while its
    // application manifest and visible app name are ChatGPT. Do not invent a
    // separate Codex activation ID based on that legacy package identity.
    if (HasPackage(L"OpenAI.Codex_2p2nqsd0c76g0"))
        result.push_back({L"ChatGPT / Codex", TargetKind::Aumid,
            L"OpenAI.Codex_2p2nqsd0c76g0!App", {}, {}});
    else
        result.push_back(InstalledTarget(L"ChatGPT / Codex", shortcuts,
            {L"chatgpt"}));
    if (HasPackage(L"Claude_pzs8sxrjxfjjc"))
        result.push_back({L"Claude Desktop", TargetKind::Aumid, L"Claude_pzs8sxrjxfjjc!Claude", {}, {}});
    else
        result.push_back(InstalledTarget(L"Claude Desktop", shortcuts,
            {L"claude", L"claude desktop"}));
    result.push_back(InstalledTarget(L"Claude Science", shortcuts,
        {L"claude science", L"claude for science"}));
    result.push_back(InstalledTarget(L"Antigravity", shortcuts,
        {L"antigravity", L"google antigravity"}, L"Programs\\Antigravity\\Antigravity.exe"));
    result.push_back(InstalledTarget(L"ZCode", shortcuts,
        {L"zcode"}, L"Programs\\ZCode\\ZCode.exe"));
    result.push_back(InstalledTarget(L"DeepSeek Harness (DSH)", shortcuts,
        {L"deepseek harness", L"deepseek-harness", L"dsh"}));
    result.push_back(InstalledTarget(L"Visual Studio Code", shortcuts,
        {L"visual studio code"}, L"Programs\\Microsoft VS Code\\Code.exe"));
    bool foundIde = false;
    for (const auto& shortcut : shortcuts) {
        const auto stem = Lower(shortcut.stem().wstring());
        for (const auto prefix : {L"intellij idea", L"pycharm", L"webstorm", L"rider", L"clion",
                                  L"goland", L"phpstorm", L"rubymine", L"rustrover", L"jetbrains air"}) {
            if (stem.starts_with(prefix) && stem.find(L"uninstall") == std::wstring::npos) {
                result.push_back({shortcut.stem().wstring(), TargetKind::Shell, shortcut.wstring(), {}, {}});
                foundIde = true;
                break;
            }
        }
    }
    if (!foundIde) result.push_back({L"JetBrains IDE", TargetKind::Shell, {}, {}, {}});
    // Junie is available in several hosts. Let users choose their actual host
    // rather than inventing a standalone Junie URI or assuming an IDE plugin.
    result.push_back(InstalledTarget(L"Junie", shortcuts, {L"junie", L"junie local"}));
    return result;
}
} // namespace reel
