#pragma once
#include "localization.h"
#include "vi_tokens.h"

#include <dwrite_2.h>
#include <filesystem>
#include <string_view>

// The reel's CJK typography (design/vi/tokens.json, typography.cjk and typography.japanese).
// One custom font fallback serves every D2D text format, the reel's and the quick menu's, and the
// text's locale picks the mapping: ja-JP text maps Han, CJK punctuation and kana to the bundled
// Reel Sans JP, then to Yu Gothic UI for characters outside its subset; other text maps kana
// the same way and Han to Microsoft YaHei UI, which keeps mainland forms. The system fallback
// follows for everything else.
namespace reel {
// True when the text contains kana: such a name is Japanese whatever the interface language.
bool HasKana(std::wstring_view text) noexcept;

// The locale a name is laid out with: ja-JP when it contains kana, so its kanji take Japanese
// forms in every interface language; otherwise the interface's own.
inline const wchar_t* TextLocale(std::wstring_view text, Language language) noexcept {
    return HasKana(text) ? vi::kJapaneseLocale : LocaleName(language);
}

// The folder of the running executable; vi::kJapaneseFontFiles are relative to it.
std::filesystem::path ExecutableFolder();

// Builds the fallback described above. Without the bundled faces under `root` (a missing or
// unreadable file) the Japanese mappings use Yu Gothic UI alone.
HRESULT CreateCjkFallback(IDWriteFactory2* factory, const std::filesystem::path& root, IDWriteFontFallback** fallback) noexcept;

// The GDI font of native controls (the quick menu's controls and the application settings
// dialog): Segoe UI with the system's font linking, or Yu Gothic UI in Japanese so kanji take
// Japanese forms. GDI does not see the bundled faces.
inline const wchar_t* ControlFamily(Language language) {
    return language == Language::Japanese ? vi::kJapaneseSystemFamily : L"Segoe UI";
}
}
