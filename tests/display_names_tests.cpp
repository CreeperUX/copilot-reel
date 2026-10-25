#include "display_names.h"
#include "behavior.h"
#include <iostream>
#include <cstdlib>

int main() {
    using namespace reel;
    unsigned checks=0;
    const auto check=[&](bool okay,const char* label){++checks;if(!okay){std::cerr<<"FAIL: "<<label<<'\n';std::exit(1);}};
    for(const auto language:{Language::English,Language::SimplifiedChinese,Language::Japanese,Language::English}) {
        SetLanguage(language);
        for(const auto* name:{L"Notepad",L"记事本",L"メモ帳"}) {
            const AppTarget app{name,TargetKind::Shell,L"notepad.exe",L"",L""};
            const auto identity=StableTargetKey(app);
            check(AppDisplayName(app)==Tr(L"Notepad",L"记事本",L"メモ帳"),"built-in notepad follows the interface language");
            check(app.name==name && identity==StableTargetKey(app),"translation cannot change stored name or launch identity");
        }
        for(const auto* name:{L"Windows Settings",L"Windows 设置",L"Windows设置",L"设置",L"Windows の設定",L"設定"}) {
            const AppTarget app{name,TargetKind::Shell,L"ms-settings:colors",L"",L""};
            check(AppDisplayName(app)==Tr(L"Windows Settings",L"Windows 设置",L"Windows の設定"),"legacy system labels follow the interface language");
            check(app.name==name,"legacy labels remain untouched in the profile");
        }
        for(const auto& app:{AppTarget{L"Project notes",TargetKind::Shell,L"notepad.exe",L"",L""},
            AppTarget{L"研究笔记",TargetKind::Shell,L"notepad.exe",L"",L""},
            AppTarget{L"研究メモ",TargetKind::Shell,L"notepad.exe",L"",L""},
            AppTarget{L"Windows 设置",TargetKind::Shell,L"custom.exe",L"",L""},
            AppTarget{L"ChatGPT",TargetKind::Aumid,L"OpenAI.Codex_2p2nqsd0c76g0!App",L"",L""}})
            check(AppDisplayName(app)==app.name,"custom labels and official product names are preserved");
        check(AppDisplayName({L"记事本",TargetKind::Aumid,L"Microsoft.WindowsNotepad_8wekyb3d8bbwe!App",L"",L""})==Tr(L"Notepad",L"记事本",L"メモ帳"),
            "packaged system targets share the same display translation");
    }
    SetLanguage(Language::Japanese);
    check(AppDisplayName({L"Notepad",TargetKind::Shell,L"notepad.exe",L"",L""})==L"メモ帳","Japanese names the built-in Notepad");
    check(StepLanguage(Language::English,1)==Language::SimplifiedChinese && StepLanguage(Language::SimplifiedChinese,1)==Language::Japanese &&
        StepLanguage(Language::Japanese,1)==Language::English && StepLanguage(Language::English,-1)==Language::Japanese,
        "the language steps EN, ZH, JA and wraps both ways");
    SetLanguage(Language::English);
    std::cout<<"Display name tests passed: "<<checks<<" checks; shared profile, locale-independent identity.\n";
}
