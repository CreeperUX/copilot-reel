#include "local_launch.h"

#include <shobjidl.h>
#include <winrt/base.h>
#include <array>
#include <filesystem>
#include <set>

namespace reel {
namespace {
bool HasShortcutExtension(const std::wstring& path) {
    return LaunchIdentity(path).ends_with(L".lnk");
}

bool IsFile(const std::wstring& path) {
    const DWORD attributes=GetFileAttributesW(path.c_str());
    return attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&FILE_ATTRIBUTE_DIRECTORY);
}

bool VerifyShortcut(std::wstring path,unsigned remaining,std::set<std::wstring>& visited) {
    if(!remaining)return false;
    std::array<wchar_t,32768> absolute{};
    const DWORD size=GetFullPathNameW(path.c_str(),static_cast<DWORD>(absolute.size()),absolute.data(),nullptr);
    if(!size || size>=absolute.size())return false;
    path=absolute.data();
    if(!IsFile(path) || !visited.insert(LaunchIdentity(path)).second)return false;

    winrt::com_ptr<IShellLinkW> link;
    if(FAILED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(link.put()))))return false;
    auto persisted=link.as<IPersistFile>();
    if(FAILED(persisted->Load(path.c_str(),STGM_READ)))return false;
    std::array<wchar_t,32768> raw{},expanded{};
    WIN32_FIND_DATAW metadata{};
    // Do not call Resolve: verification must not show UI, search the network,
    // update the shortcut, or execute the destination while choosing a target.
    if(FAILED(link->GetPath(raw.data(),static_cast<int>(raw.size()),&metadata,SLGP_RAWPATH)) || !raw[0])return false;
    const DWORD required=ExpandEnvironmentStringsW(raw.data(),expanded.data(),static_cast<DWORD>(expanded.size()));
    if(!required || required>expanded.size())return false;
    AppTarget destination{L"",TargetKind::Shell,expanded.data(),L"",L""};
    if(!IsLocalApplication(destination))return false;
    std::filesystem::path resolved(destination.target);
    if(resolved.is_relative())resolved=std::filesystem::path(path).parent_path()/resolved;
    destination.target=resolved.lexically_normal().wstring();
    if(HasShortcutExtension(destination.target))return VerifyShortcut(destination.target,remaining-1,visited);
    return LaunchIdentity(destination.target).ends_with(L".exe") && IsFile(destination.target);
}
}

bool IsVerifiedLocalApplication(const AppTarget& app) {
    if(!IsLocalApplication(app))return false;
    if(app.kind!=TargetKind::Shell || !HasShortcutExtension(app.target))return true;
    try {
        std::set<std::wstring> visited;
        return VerifyShortcut(app.target,4,visited);
    } catch(...) {return false;}
}
}
