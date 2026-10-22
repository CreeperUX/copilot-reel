#pragma once
#include "app_types.h"
#include "localization.h"
#include <algorithm>
#include <functional>
#include <set>

namespace reel {
enum class MenuPage { Main, Manage, Presets, Settings, Preferred, Entry };
enum class MenuAction { Launch, Manage, Settings, Edit, Presets, Preset, Custom,
    Language, Preferred, SetPreferred, Appearance, Entry, Remove, Previous, Next, Reorder };
// Disabled entries keep their slot and name but can never be chosen.
struct MenuEntry { AppTarget visual; MenuAction action; int index=-1; bool enabled=true; };
inline constexpr size_t kMaxApps=8;

inline bool CanConfirm(const std::vector<MenuEntry>& menu,int index) {
    return index>=0 && static_cast<size_t>(index)<menu.size() && menu[index].enabled;
}

// Slot 0 of the hub (the language) and of an app's actions page (its place in the reel)
// is adjusted with the flanking chevrons or A/D. A side that cannot move is disabled.
struct SlotAdjust { bool control=false,earlier=false,later=false; };
inline SlotAdjust AdjustFor(MenuPage page,size_t count,int editing) {
    if(page==MenuPage::Settings)return {true,true,true};
    if(page==MenuPage::Entry && count>1 && editing>=0 && static_cast<size_t>(editing)<count)
        return {true,editing>0,static_cast<size_t>(editing)+1<count};
    return {};
}

// Moves one app a single step and keeps every field of every entry.
inline bool MoveEntry(std::vector<AppTarget>& apps,int index,int delta) {
    if((delta!=-1 && delta!=1) || index<0 || static_cast<size_t>(index)>=apps.size())return false;
    const int other=index+delta;
    if(other<0 || static_cast<size_t>(other)>=apps.size())return false;
    std::swap(apps[index],apps[other]);return true;
}
// Where an entry at `position` lands after MoveEntry(apps,index,delta).
inline int MovedIndex(int position,int index,int delta) {
    if(position==index)return index+delta;
    if(position==index+delta)return index;
    return position;
}

// The Add page: installed agents that are not in the reel yet, in catalog order, then
// catalog entries that still need a target. `identity` names the application a target
// launches and is empty when the target is not a verified local application.
inline std::vector<AppTarget> AddCatalog(const std::vector<AppTarget>& apps,const std::vector<AppTarget>& presets,
    const std::function<std::wstring(const AppTarget&)>& identity,size_t* installedCount=nullptr) {
    std::set<std::wstring> taken;
    for(const auto& app:apps)if(auto key=identity(app);!key.empty())taken.insert(std::move(key));
    std::vector<AppTarget> installed,pending;
    for(const auto& preset:presets) {
        auto key=identity(preset);
        if(key.empty())pending.push_back(preset);
        else if(taken.insert(std::move(key)).second)installed.push_back(preset);
    }
    if(installedCount)*installedCount=installed.size();
    installed.insert(installed.end(),pending.begin(),pending.end());
    return installed;
}

// Esc and Backspace go back, Return and Space confirm, Tab moves around the ring. On a page
// with an adjustable slot 0, A and D always adjust it; Left and Right adjust it while it is
// selected and otherwise move around the ring like on every other page.
enum class KeyRoute { None, Back, Confirm, Adjust, Next, Previous };
inline KeyRoute RouteKey(UINT key,bool shift,bool adjustable,int selected) {
    if(key==VK_ESCAPE || key==VK_BACK)return KeyRoute::Back;
    if(key==VK_RETURN || key==VK_SPACE)return KeyRoute::Confirm;
    if(adjustable && (key=='A' || key=='D'))return KeyRoute::Adjust;
    if(key==VK_LEFT || key==VK_RIGHT)
        return adjustable && selected==0?KeyRoute::Adjust:(key==VK_LEFT?KeyRoute::Previous:KeyRoute::Next);
    if(key==VK_TAB)return shift?KeyRoute::Previous:KeyRoute::Next;
    return KeyRoute::None;
}
inline int AdjustDirection(UINT key) {return key=='A' || key==VK_LEFT?-1:1;}

// Action metadata is separate from launch targets, so internal commands never
// reach ShellExecute and app entries retain their official icon identity.
inline std::vector<MenuEntry> BuildMenu(MenuPage page,const std::vector<AppTarget>& apps,
    const std::vector<AppTarget>& presets,size_t offset=0,int editing=-1) {
    std::vector<MenuEntry> result;
    auto action=[&](const wchar_t* name,const wchar_t* glyph,MenuAction kind,int index=-1,bool enabled=true) {
        result.push_back({{name,TargetKind::Shell,glyph,L"",L""},kind,index,enabled});
    };
    auto entries=[&](MenuAction kind) {
        for(size_t i=0;i<apps.size();++i) result.push_back({apps[i],kind,static_cast<int>(i)});
    };
    // At capacity Add stays in place, disabled, so the orbit never reflows.
    const bool canAdd=apps.size()<kMaxApps;
    switch(page) {
    case MenuPage::Main:
        entries(MenuAction::Launch);
        action(Tr(L"Apps and settings",L"应用与设置"),L"reel:add",MenuAction::Settings);break;
    case MenuPage::Manage:
        entries(MenuAction::Entry);
        action(Tr(L"Add app",L"添加应用"),L"reel:add",MenuAction::Presets,-1,canAdd);break;
    case MenuPage::Presets:
        for(size_t i=offset;i<std::min(offset+5,presets.size());++i)
            result.push_back({presets[i],MenuAction::Preset,static_cast<int>(i)});
        action(Tr(L"Custom app",L"自定义应用"),L"reel:add",MenuAction::Custom);
        if(offset>0)action(Tr(L"Previous",L"上一页"),L"reel:previous",MenuAction::Previous);
        if(offset+5<presets.size())action(Tr(L"Next",L"下一页"),L"reel:next",MenuAction::Next);break;
    case MenuPage::Settings:
        action(Tr(L"Language",L"语言"),L"reel:language",MenuAction::Language);
        action(Tr(L"Appearance",L"外观"),L"reel:appearance",MenuAction::Appearance);
        action(Tr(L"Preferred agent",L"首选应用"),L"reel:preferred",MenuAction::Preferred);
        action(Tr(L"Manage apps",L"管理应用"),L"reel:manage",MenuAction::Manage);
        action(Tr(L"Add app",L"添加应用"),L"reel:add",MenuAction::Presets,-1,canAdd);break;
    case MenuPage::Preferred:entries(MenuAction::SetPreferred);break;
    case MenuPage::Entry:
        // The app itself comes first: its chevrons move it, and choosing it is done.
        if(editing>=0 && static_cast<size_t>(editing)<apps.size())result.push_back({apps[editing],MenuAction::Reorder,editing});
        action(Tr(L"Edit app",L"编辑应用"),L"reel:edit",MenuAction::Edit,editing);
        action(Tr(L"Set as preferred",L"设为首选"),L"reel:preferred",MenuAction::SetPreferred,editing);
        if(apps.size()>1)action(Tr(L"Remove from reel",L"从轮盘移除"),L"reel:remove",MenuAction::Remove,editing);break;
    }
    return result;
}
}
