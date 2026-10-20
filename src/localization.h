#pragma once

namespace reel {
enum class Language { English, SimplifiedChinese };

Language CurrentLanguage() noexcept;
// BCP 47 locale for text shaping and diagnostics.
inline const wchar_t* LocaleName(Language language) noexcept {
    return language == Language::SimplifiedChinese ? L"zh-CN" : L"en-US";
}
void SetLanguage(Language language) noexcept;
const wchar_t* Tr(const wchar_t* english, const wchar_t* chinese) noexcept;
// Missing or invalid preferences select English. Loading also sets the current language.
Language LoadLanguage() noexcept;
// Preferences are independent of apps.ini and use the same data-directory override.
bool SaveLanguage(Language language) noexcept;
}
