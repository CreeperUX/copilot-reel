#include "localization.h"
#include "settings.h"

#include <array>
#include <atomic>
#include <string>
#include <string_view>

namespace reel {
namespace {
std::atomic<Language> currentLanguage{Language::English};

std::wstring PreferencesPath() {
    const auto config = ConfigPath();
    if (config.empty()) return {};
    const auto separator = config.find_last_of(L"\\/");
    if (separator == std::wstring::npos) return {};
    return config.substr(0, separator + 1) + L"preferences.ini";
}
}

Language CurrentLanguage() noexcept { return currentLanguage.load(std::memory_order_relaxed); }

void SetLanguage(Language language) noexcept {
    currentLanguage.store(language == Language::SimplifiedChinese ? language : Language::English,
        std::memory_order_relaxed);
}

const wchar_t* Tr(const wchar_t* english, const wchar_t* chinese) noexcept {
    return CurrentLanguage() == Language::SimplifiedChinese ? chinese : english;
}

Language LoadLanguage() noexcept {
    Language language = Language::English;
    try {
        const auto path = PreferencesPath();
        if (!path.empty()) {
            std::array<wchar_t, 32> value{};
            GetPrivateProfileStringW(L"General", L"Language", L"en-US", value.data(),
                static_cast<DWORD>(value.size()), path.c_str());
            if (std::wstring_view(value.data()) == L"zh-CN") language = Language::SimplifiedChinese;
        }
    } catch (...) { }
    SetLanguage(language);
    return language;
}

bool SaveLanguage(Language language) noexcept {
    std::wstring temporary;
    HANDLE file=INVALID_HANDLE_VALUE;
    bool owned=false;
    try {
        const auto path = PreferencesPath();
        if (path.empty()) return false;
        const auto directory = path.substr(0, path.find_last_of(L"\\/"));
        if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
        const wchar_t* value = language == Language::SimplifiedChinese ? L"zh-CN" : L"en-US";
        const std::wstring content=std::wstring(L"\xfeff[General]\r\nLanguage=")+value+L"\r\n";
        temporary=path+L"."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64())+L".tmp";
        file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE) return false;
        owned=true;
        const DWORD bytes=static_cast<DWORD>(content.size()*sizeof(wchar_t));
        DWORD written=0;
        const bool saved=WriteFile(file,content.data(),bytes,&written,nullptr) && written==bytes && FlushFileBuffers(file);
        CloseHandle(file);file=INVALID_HANDLE_VALUE;
        if(!saved || !MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
            DeleteFileW(temporary.c_str());return false;
        }
        owned=false;
        // Invalidate the profile-API cache after the atomic replacement. This
        // cache-control call's return value does not describe our file write.
        WritePrivateProfileStringW(nullptr,nullptr,nullptr,path.c_str());
        return true;
    } catch (...) {
        if(file!=INVALID_HANDLE_VALUE) CloseHandle(file);
        if(owned) DeleteFileW(temporary.c_str());
        return false;
    }
}
}
