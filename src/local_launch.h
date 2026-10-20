#pragma once
#include "app_types.h"
#include <algorithm>
#include <cwctype>
#include <optional>
#include <string_view>

namespace reel {
// Checks shortcut destinations without executing or rewriting them. Requires
// COM on the calling thread; unresolved, cyclic, and non-application links fail.
bool IsVerifiedLocalApplication(const AppTarget& app);

inline std::wstring LaunchIdentity(std::wstring value) {
    std::transform(value.begin(),value.end(),value.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
    return value;
}

// Short tap is an application activation, never a browser fallback. Explicit
// website entries may remain in the wheel, but cannot be dispatched by Tap.
inline bool IsLocalApplication(const AppTarget& app) {
    const auto target=LaunchIdentity(app.target);
    if(app.kind==TargetKind::Aumid)return !target.empty() && target.find(L'!')!=target.npos && target.find(L':')==target.npos;
    if(target.starts_with(L"ms-settings:"))return true;
    if(target.starts_with(L"shell:appsfolder\\") && target.find(L'!')!=target.npos)return true;
    if(target.find(L"://")!=target.npos || target.find_first_of(L"?\r\n")!=target.npos)return false;
    const auto colon=target.find(L':');
    if(colon!=target.npos && (colon!=1 || target.size()<3 || (target[2]!=L'\\' && target[2]!=L'/')))return false;
    return target.ends_with(L".exe") || target.ends_with(L".lnk");
}

inline std::optional<AppTarget> LocalReplacement(const AppTarget& source,const std::vector<AppTarget>& presets) {
    if(IsLocalApplication(source))return source;
    if(source.kind!=TargetKind::Shell || !source.arguments.empty() || !source.workingDirectory.empty())return {};
    auto target=LaunchIdentity(source.target);
    if(target.ends_with(L"/"))target.pop_back();
    std::wstring_view name;
    if(target==L"https://chatgpt.com" || target==L"https://chat.openai.com")name=L"ChatGPT / Codex";
    else if(target==L"https://claude.ai")name=L"Claude Desktop";
    else return {};
    for(const auto& preset:presets)if(preset.name==name && IsLocalApplication(preset)) {
        auto result=preset;result.name=source.name;return result;
    }
    return {};
}
}
