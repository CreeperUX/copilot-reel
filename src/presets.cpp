#include "presets.h"

#include "agent_catalog.h"
#include "local_launch.h"

#include <appmodel.h>
#include <shlobj.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <set>
#include <string_view>

namespace reel {
namespace {
using catalog::Rule;

std::wstring Folder(REFKNOWNFOLDERID id) {
    PWSTR value = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &value))) return {};
    std::wstring result(value);
    CoTaskMemFree(value);
    return result;
}

bool HasPackage(std::wstring_view aumid) {
    const std::wstring family(aumid.substr(0, aumid.find(L'!')));
    UINT32 count = 0, length = 0;
    const LONG status = GetPackagesByPackageFamily(family.c_str(), &count, nullptr, &length, nullptr);
    return status == ERROR_INSUFFICIENT_BUFFER && count != 0;
}

std::wstring Expand(const std::wstring& value) {
    if (value.find(L'%') == std::wstring::npos) return value;
    std::wstring result(32768, L'\0');
    const DWORD size = ExpandEnvironmentStringsW(value.c_str(), result.data(), static_cast<DWORD>(result.size()));
    if (!size || size > result.size()) return {};
    result.resize(size - 1);
    return result;
}

// Absolute paths only; this also normalizes separators and dot segments.
std::wstring FullPath(const std::wstring& path) {
    if (path.empty() || !std::filesystem::path(path).is_absolute()) return {};
    DWORD size = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (!size) return {};
    std::wstring result(size, L'\0');
    size = GetFullPathNameW(path.c_str(), size, result.data(), nullptr);
    if (!size || size >= result.size()) return {};
    result.resize(size);
    return result;
}

// Expands 8.3 short names; empty when the file does not exist.
std::wstring LongPath(const std::wstring& path) {
    std::wstring result(32768, L'\0');
    const DWORD size = GetLongPathNameW(path.c_str(), result.data(), static_cast<DWORD>(result.size()));
    if (!size || size >= result.size()) return {};
    result.resize(size);
    return result;
}

// The PE optional-header subsystem, or 0 for a missing file, a reparse point such as a
// 0-byte app execution alias, or anything that is not an image.
unsigned ImageSubsystem(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) return 0;
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (file == INVALID_HANDLE_VALUE) return 0;
    auto read = [file](LONG offset, void* data, DWORD size) {
        DWORD done = 0;
        return SetFilePointer(file, offset, nullptr, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
            ReadFile(file, data, size, &done, nullptr) && done == size;
    };
    // Signature, file header, and the optional header up to Subsystem, which sits at the
    // same offset in PE32 and PE32+.
    static_assert(offsetof(IMAGE_OPTIONAL_HEADER32, Subsystem) == offsetof(IMAGE_OPTIONAL_HEADER64, Subsystem));
    constexpr size_t optional = sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
    constexpr size_t subsystemAt = optional + offsetof(IMAGE_OPTIONAL_HEADER64, Subsystem);
    std::array<BYTE, subsystemAt + sizeof(WORD)> headers{};
    IMAGE_DOS_HEADER dos{};
    unsigned subsystem = 0;
    if (read(0, &dos, sizeof(dos)) && dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew >= static_cast<LONG>(sizeof(dos)) &&
        read(dos.e_lfanew, headers.data(), static_cast<DWORD>(headers.size()))) {
        DWORD signature = 0;
        IMAGE_FILE_HEADER header{};
        WORD value = 0;
        std::memcpy(&signature, headers.data(), sizeof(signature));
        std::memcpy(&header, headers.data() + sizeof(signature), sizeof(header));
        std::memcpy(&value, headers.data() + subsystemAt, sizeof(value));
        if (signature == IMAGE_NT_SIGNATURE && static_cast<size_t>(header.SizeOfOptionalHeader) >= subsystemAt - optional + sizeof(WORD))
            subsystem = value;
    }
    CloseHandle(file);
    return subsystem;
}

struct Installed {
    const Rule* rule;
    std::wstring path, stem;
};

// Collects installed catalog executables from every source, keyed by the lowercase
// long path, so two shortcuts, an Uninstall entry and a folder probe of one exe
// make one install. The first source to name an exe wins.
class Discovery {
public:
    explicit Discovery(const std::array<bool, std::size(catalog::kEntries)>& packaged) : packaged_(packaged) {}
    // Returns whether `raw` names a rule's executable, installed or not. The pure
    // match runs first, so only catalog candidates touch the disk.
    bool Consider(const std::wstring& raw, std::wstring_view stem = {}) {
        auto path = Expand(raw);
        // An 8.3 short name hides the product's file and folder names.
        if (path.find(L'~') != std::wstring::npos) path = LongPath(path);
        path = FullPath(path);
        const Rule* rule = catalog::MatchRule(path);
        if (!rule) return false;
        if (packaged_[static_cast<size_t>(rule->agent)] || !examined_.insert(catalog::Lower(path)).second) return true;
        if (!catalog::MatchCandidate(path, ImageSubsystem(path))) return true;
        auto stored = LongPath(path);
        if (stored.empty()) return true;
        if (const auto key = catalog::Lower(stored); key != catalog::Lower(path) && !examined_.insert(key).second) return true;
        installed.push_back({rule, std::move(stored), std::wstring(stem)});
        return true;
    }
    std::vector<Installed> installed;

private:
    std::array<bool, std::size(catalog::kEntries)> packaged_;
    std::set<std::wstring> examined_;
};

// Whether a shortcut may point at a catalog executable, from its raw bytes. A link too
// large or unreadable here is left to IShellLink.
bool MayNameRuleExe(const std::wstring& link, std::string& buffer) {
    const HANDLE file = CreateFileW(link.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) return true;
    buffer.resize(64 * 1024);
    DWORD size = 0;
    const bool read = ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &size, nullptr) && size < buffer.size();
    CloseHandle(file);
    return !read || catalog::MayNameRuleExe(std::string_view(buffer.data(), size));
}

// Start-menu shortcuts in both Programs roots, read without Resolve. The shortcut is
// only a pointer: its stem names an IDE row, and the executable is what gets stored.
void ReadShortcuts(Discovery& discovery) {
    std::string buffer;
    for (const auto id : {&FOLDERID_Programs, &FOLDERID_CommonPrograms}) {
        const auto root = Folder(*id);
        if (root.empty()) continue;
        std::error_code error;
        std::filesystem::recursive_directory_iterator iterator(root,
            std::filesystem::directory_options::skip_permission_denied, error), end;
        for (; iterator != end && !error; iterator.increment(error)) {
            const auto& path = iterator->path();
            if (catalog::Same(path.extension().native(), L".lnk") && MayNameRuleExe(path.native(), buffer))
                discovery.Consider(ShortcutTarget(path.native()), path.stem().native());
        }
    }
}

std::wstring ReadString(HKEY key, const wchar_t* name) {
    constexpr DWORD flags = RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND;
    DWORD size = 0;
    if (RegGetValueW(key, nullptr, name, flags, nullptr, nullptr, &size) != ERROR_SUCCESS || size < sizeof(wchar_t)) return {};
    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(key, nullptr, name, flags, nullptr, value.data(), &size) != ERROR_SUCCESS) return {};
    value.resize(wcsnlen(value.c_str(), value.size()));
    return value;
}

// Uninstall entries in HKCU, HKLM and HKLM\WOW6432Node: the exe from DisplayIcon, else
// InstallLocation plus each matching rule's exe. HKLM entries can point into one user's
// profile, so every candidate is still checked on disk for the current user.
void ReadUninstallEntries(Discovery& discovery) {
    constexpr wchar_t kUninstall[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
    const struct { HKEY root; REGSAM view; } hives[] = {
        {HKEY_CURRENT_USER, 0}, {HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY}, {HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY}};
    for (const auto& hive : hives) {
        HKEY uninstall = nullptr;
        if (RegOpenKeyExW(hive.root, kUninstall, 0, KEY_READ | hive.view, &uninstall) != ERROR_SUCCESS) continue;
        std::array<wchar_t, 256> name{};  // registry key names are at most 255 characters
        for (DWORD index = 0;; ++index) {
            DWORD length = static_cast<DWORD>(name.size());
            const LONG status = RegEnumKeyExW(uninstall, index, name.data(), &length, nullptr, nullptr, nullptr, nullptr);
            if (status == ERROR_NO_MORE_ITEMS) break;
            HKEY entry = nullptr;
            if (status != ERROR_SUCCESS || RegOpenKeyExW(uninstall, name.data(), 0, KEY_QUERY_VALUE | hive.view, &entry) != ERROR_SUCCESS)
                continue;
            const auto icon = catalog::ExeFromDisplayIcon(ReadString(entry, L"DisplayIcon"));
            auto location = Expand(ReadString(entry, L"InstallLocation"));
            RegCloseKey(entry);
            if (!icon.empty() && discovery.Consider(icon)) continue;
            if (location.size() >= 2 && location.front() == L'"' && location.back() == L'"') location = location.substr(1, location.size() - 2);
            while (!location.empty() && (location.back() == L'\\' || location.back() == L'/')) location.pop_back();
            if (location.empty()) continue;
            const auto folder = catalog::Split(location).file;
            for (const auto& rule : catalog::kRules)
                if (catalog::FolderMatches(rule, folder))
                    discovery.Consider(location + (rule.ide ? L"\\bin\\" : L"\\") + std::wstring(rule.exe));
        }
        RegCloseKey(uninstall);
    }
}

// Exact install folders only; nothing is searched recursively.
void ProbeFolders(Discovery& discovery) {
    const auto local = Folder(FOLDERID_LocalAppData), programFiles = Folder(FOLDERID_ProgramFiles);
    for (const auto& rule : catalog::kRules) {
        const auto file = std::wstring(rule.folder) + L'\\' + std::wstring(rule.exe);
        if (rule.probe == catalog::Probe::Programs) {
            if (!local.empty()) discovery.Consider(local + L"\\Programs\\" + file);
            if (!programFiles.empty()) discovery.Consider(programFiles + L'\\' + file);
        } else if (rule.probe == catalog::Probe::LocalAppData && !local.empty()) {
            discovery.Consider(local + L'\\' + file);
        }
    }
}
} // namespace

std::vector<AppTarget> AgentPresets() {
    std::array<bool, std::size(catalog::kEntries)> packaged{};
    for (size_t index = 0; index < packaged.size(); ++index)
        packaged[index] = !catalog::kEntries[index].aumid.empty() && HasPackage(catalog::kEntries[index].aumid);
    Discovery discovery(packaged);
    // A failing source only hides what it would have found; core rows still appear.
    try { ReadShortcuts(discovery); } catch (...) {}
    try { ReadUninstallEntries(discovery); } catch (...) {}
    try { ProbeFolders(discovery); } catch (...) {}

    std::vector<AppTarget> result;
    for (size_t index = 0; index < packaged.size(); ++index) {
        const auto& entry = catalog::kEntries[index];
        if (packaged[index]) {
            result.push_back({std::wstring(entry.name), TargetKind::Aumid, std::wstring(entry.aumid), {}, {}});
            continue;
        }
        const auto first = result.size();
        for (const auto& rule : catalog::kRules) {
            if (static_cast<size_t>(rule.agent) != index) continue;
            for (const auto& app : discovery.installed) {
                if (app.rule != &rule) continue;
                // IDEs keep one row per install, named by its shortcut or its versioned folder.
                // Other products and channels keep one row each; the first source wins.
                std::wstring name = rule.ide ? (app.stem.empty() ? std::wstring(catalog::InstallFolder(app.path, rule)) : app.stem)
                                             : std::wstring(catalog::RowName(rule));
                if (!rule.ide && std::any_of(result.begin() + first, result.end(),
                        [&](const AppTarget& row) { return row.name == name; })) break;
                result.push_back({std::move(name), TargetKind::Shell, app.path, {}, {}});
                if (!rule.ide) break;
            }
        }
        if (result.size() == first && entry.core) result.push_back({std::wstring(entry.name), TargetKind::Shell, {}, {}, {}});
    }
    return result;
}
} // namespace reel
