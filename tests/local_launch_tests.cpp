#include "local_launch.h"
#include <shobjidl.h>
#include <shlobj.h>
#include <winrt/base.h>
#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks=0;
void Check(bool value,const char* label) {
    ++checks;if(!value)throw std::runtime_error(label);
}
struct Fixture {
    std::wstring directory;
    std::vector<std::wstring> files;
    Fixture() {
        std::array<wchar_t,MAX_PATH+1> temp{},unique{};
        Check(GetTempPathW(static_cast<DWORD>(temp.size()),temp.data())>0,"temporary root exists");
        Check(GetTempFileNameW(temp.data(),L"CRL",0,unique.data())!=0,"unique owned test file created");
        directory=unique.data();
        Check(DeleteFileW(directory.c_str()) && CreateDirectoryW(directory.c_str(),nullptr),"owned fixture directory created");
    }
    ~Fixture() {
        for(const auto& file:files)DeleteFileW(file.c_str());
        RemoveDirectoryW(directory.c_str());
    }
    std::wstring Path(const wchar_t* name) const {return directory+L"\\"+name;}
    std::wstring Link(const wchar_t* name,const std::wstring& destination) {
        const auto path=Path(name);

        winrt::com_ptr<IShellLinkW> link;
        winrt::check_hresult(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(link.put())));
        if(destination.starts_with(L"https://")) {
            PIDLIST_ABSOLUTE item=nullptr;
            winrt::check_hresult(SHParseDisplayName(destination.c_str(),nullptr,&item,0,nullptr));
            const HRESULT result=link->SetIDList(item);CoTaskMemFree(item);
            winrt::check_hresult(result);
        } else winrt::check_hresult(link->SetPath(destination.c_str()));
        winrt::check_hresult(link.as<IPersistFile>()->Save(path.c_str(),TRUE));
        files.push_back(path);return path;
    }
};
reel::AppTarget Target(const std::wstring& path) {return {L"Test app",reel::TargetKind::Shell,path,L"",L""};}
}

int main() {
    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        Fixture fixture;
        std::array<wchar_t,32768> windows{};
        Check(GetWindowsDirectoryW(windows.data(),static_cast<UINT>(windows.size()))>0,"Windows path exists");
        const auto notepad=std::wstring(windows.data())+L"\\System32\\notepad.exe";
        Check(GetFileAttributesW(notepad.c_str())!=INVALID_FILE_ATTRIBUTES,"native Notepad test destination exists");
        const auto native=fixture.Link(L"native.lnk",notepad);
        auto profile=Target(native);profile.arguments=L"--preserved";
        Check(reel::IsVerifiedLocalApplication(profile),"native executable shortcut accepted");
        Check(profile.target==native && profile.arguments==L"--preserved","verification does not rewrite launch profile");
        const auto second=fixture.Link(L"second.lnk",native);
        const auto third=fixture.Link(L"third.lnk",second);
        Check(reel::IsVerifiedLocalApplication(Target(second)),"native shortcut chain accepted");
        // ShellLink may flatten chains while saving. Test the actual resulting
        // native destination rather than assuming an unresolved on-disk chain.
        Check(reel::IsVerifiedLocalApplication(Target(third)),"ShellLink-saved native chain remains usable");
        const auto website=fixture.Link(L"website.lnk",L"https://example.com/");
        Check(!reel::IsVerifiedLocalApplication(Target(website)),"URL destination shortcut rejected");
        const auto missing=fixture.Link(L"missing-target.lnk",fixture.Path(L"missing.exe"));
        Check(!reel::IsVerifiedLocalApplication(Target(missing)),"missing shortcut destination rejected");
        Check(!reel::IsVerifiedLocalApplication(Target(fixture.Path(L"missing.lnk"))),"missing shortcut file rejected");
        Check(!reel::IsVerifiedLocalApplication(Target(L"https://example.com/")),"direct web target rejected");
        Check(reel::IsVerifiedLocalApplication(Target(L"notepad.exe")),"non-shortcut native launch policy retained");
        Check(reel::IsVerifiedLocalApplication(Target(L"ms-settings:colors")),"Windows settings activation retained");
        Check(reel::IsVerifiedLocalApplication({L"Package",reel::TargetKind::Aumid,L"PackageFamily!App",L"",L""}),"packaged activation policy retained");
        std::cout<<"Local launch tests passed: "<<checks<<" checks; no destination was executed.\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;
    } catch(const winrt::hresult_error& error) {
        std::wcerr<<L"FAIL: "<<error.message().c_str()<<L'\n';return 1;
    }
}
