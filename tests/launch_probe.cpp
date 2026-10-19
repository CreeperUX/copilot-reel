// Build as a separate Windows-subsystem executable. This intentionally has no
// UI and launches no applications. It writes only into the explicit test data
// directory, never the user's normal Copilot Reel configuration directory.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <array>
#include <string>
#include <string_view>

namespace {
bool AbsoluteDirectory(std::wstring_view path) {
    const auto separator = [](wchar_t c) { return c == L'\\' || c == L'/'; };
    if (path.size() >= 3 && ((path[0] >= L'A' && path[0] <= L'Z') ||
        (path[0] >= L'a' && path[0] <= L'z')) && path[1] == L':' && separator(path[2])) return true;
    if (path.size() < 5 || path[0] != L'\\' || path[1] != L'\\' || path[2] == L'.' || path[2] == L'?') return false;
    const auto share = path.find_first_of(L"\\/", 2);
    return share != std::wstring_view::npos && share > 2 && share + 1 < path.size() && !separator(path[share + 1]);
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    try {
        std::array<wchar_t, 32768> buffer{};
        const DWORD copied = GetEnvironmentVariableW(L"COPILOT_REEL_DATA_DIR", buffer.data(), static_cast<DWORD>(buffer.size()));
        if (copied == 0 || copied >= buffer.size()) return ERROR_BAD_ENVIRONMENT;
        std::wstring directory(buffer.data(), copied);
        if (!AbsoluteDirectory(directory)) return ERROR_BAD_PATHNAME;
        if (directory.back() != L'\\' && directory.back() != L'/') directory += L'\\';
        const std::wstring path = directory + L"launch-probe.txt";
        HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
        if (file == INVALID_HANDLE_VALUE) return static_cast<int>(GetLastError());
        // Append one line per invocation so duplicate dispatches are observable.
        constexpr char marker[] = "launched\n";
        DWORD written = 0;
        const bool okay = WriteFile(file, marker, sizeof(marker) - 1, &written, nullptr) &&
            written == sizeof(marker) - 1 && FlushFileBuffers(file);
        const DWORD failure = okay ? ERROR_SUCCESS : GetLastError();
        const DWORD code = okay ? ERROR_SUCCESS : (failure != ERROR_SUCCESS ? failure : ERROR_WRITE_FAULT);
        CloseHandle(file);
        return static_cast<int>(code);
    } catch (...) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }
}
