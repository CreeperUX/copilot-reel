#pragma once
#include "app_types.h"
#include "localization.h"
#include <algorithm>

namespace reel {
enum class MenuPage { Main, Manage, Presets, Settings, Preferred, Entry };
enum class MenuAction { Launch, Manage, Settings, Edit, Presets, Preset, Custom,
    Language, Preferred, SetPreferred, Appearance, Entry, Remove, Previous, Next };
struct MenuEntry { AppTarget visual; MenuAction action; int index=-1; };

// Action metadata is separate from launch targets, so internal commands never
// reach ShellExecute and app entries retain their official icon identity.
inline std::vector<MenuEntry> BuildMenu(MenuPage page,const std::vector<AppTarget>& apps,
    const std::vector<AppTarget>& presets,size_t offset=0,int editing=-1) {
    std::vector<MenuEntry> result;
    auto action=[&](const wchar_t* name,const wchar_t* glyph,MenuAction kind,int index=-1) {
        result.push_back({{name,TargetKind::Shell,glyph,L"",L""},kind,index});
    };
    auto entries=[&](MenuAction kind) {
        for(size_t i=0;i<apps.size();++i) result.push_back({apps[i],kind,static_cast<int>(i)});
    };
    switch(page) {
    case MenuPage::Main:
        entries(MenuAction::Launch);
        action(Tr(L"Add or change apps",L"添加或更改应用"),L"reel:manage",MenuAction::Manage);
        action(Tr(L"Settings",L"设置"),L"reel:settings",MenuAction::Settings);break;
    case MenuPage::Manage:
        entries(MenuAction::Entry);
        if(apps.size()<8)action(Tr(L"Add app",L"添加应用"),L"reel:add",MenuAction::Presets);break;
    case MenuPage::Presets:
        for(size_t i=offset;i<std::min(offset+5,presets.size());++i)
            result.push_back({presets[i],MenuAction::Preset,static_cast<int>(i)});
        action(Tr(L"Custom app",L"自定义应用"),L"reel:add",MenuAction::Custom);
        if(offset>0)action(Tr(L"Previous",L"上一页"),L"reel:previous",MenuAction::Previous);
        if(offset+5<presets.size())action(Tr(L"Next",L"下一页"),L"reel:next",MenuAction::Next);break;
    case MenuPage::Settings:
        action(Tr(L"Language",L"语言"),L"reel:language",MenuAction::Language);
        action(Tr(L"Preferred agent",L"首选应用"),L"reel:preferred",MenuAction::Preferred);
        action(Tr(L"Windows appearance",L"Windows 外观"),L"reel:appearance",MenuAction::Appearance);break;
    case MenuPage::Preferred:entries(MenuAction::SetPreferred);break;
    case MenuPage::Entry:
        action(Tr(L"Edit app",L"编辑应用"),L"reel:edit",MenuAction::Edit,editing);
        action(Tr(L"Set as preferred",L"设为首选"),L"reel:preferred",MenuAction::SetPreferred,editing);
        if(apps.size()>1)action(Tr(L"Remove from reel",L"从轮盘移除"),L"reel:remove",MenuAction::Remove,editing);break;
    }
    return result;
}
}
