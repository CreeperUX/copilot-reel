#pragma once
#include "iconography.h"
#include "localization.h"
#include <algorithm>
#include <cwctype>

namespace reel {
// Translate only recognized built-in default labels at presentation time.
// Stored names, launch identities, custom names, and icon inputs remain intact.
inline std::wstring AppDisplayName(const AppTarget& app) {
    auto name=app.name;
    std::transform(name.begin(),name.end(),name.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
    switch(IdentifySystemIcon(app)) {
    case SystemIconKind::Notepad:
        if(name==L"notepad" || name==L"记事本" || name==L"メモ帳")return Tr(L"Notepad",L"记事本",L"メモ帳");
        break;
    case SystemIconKind::Settings:
        if(name==L"windows settings" || name==L"settings" || name==L"windows 设置" || name==L"windows设置" || name==L"设置" ||
           name==L"windows の設定" || name==L"設定")
            return Tr(L"Windows Settings",L"Windows 设置",L"Windows の設定");
        break;
    default:break;
    }
    return app.name;
}
}
