#include "official_icons.h"
#include "vi_tokens.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

namespace reel {
namespace {
std::map<std::wstring, std::vector<BYTE>, std::less<>> sources;
bool loaded = false;

std::vector<BYTE> ReadBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) return {};
    const auto end = input.tellg();
    if (end <= 0 || end > 8 * 1024 * 1024) return {};
    std::vector<BYTE> bytes(static_cast<size_t>(end));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) return {};
    return bytes;
}
} // namespace

void PreloadOfficialIconAssets() {
    if (loaded) return;
    loaded = true;
    std::array<wchar_t, 32768> executable{};
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (length == 0 || length >= executable.size()) return;
    const auto root = std::filesystem::path(executable.data()).parent_path() / L"assets" / L"brands";
    // The SVG originals stay in the package for provenance. Every vendor asset has an
    // official or same-artwork PNG, which artwork.cpp crops and prescales per DPI.
    for (std::size_t i = 0; i < vi::kBrandCount; ++i) {
        const std::wstring_view key = vi::kBrands[i].key;
        if (key.starts_with(L"shell:")) continue;
        auto bytes = ReadBytes(root / (std::wstring(key) + L".png"));
        if (!bytes.empty()) sources.emplace(key, std::move(bytes));
    }
}

const std::vector<BYTE>* OfficialArtworkBytes(std::wstring_view key) {
    const auto found = sources.find(key);
    return found == sources.end() ? nullptr : &found->second;
}
} // namespace reel
