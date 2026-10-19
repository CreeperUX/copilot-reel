#pragma once

namespace reel {
enum class Language { English, SimplifiedChinese };

Language CurrentLanguage() noexcept;
void SetLanguage(Language language) noexcept;
const wchar_t* Tr(const wchar_t* english, const wchar_t* chinese) noexcept;
// Missing or invalid preferences select English. Loading also sets the current language.
Language LoadLanguage() noexcept;
// Preferences are independent of apps.ini and use the same data-directory override.
bool SaveLanguage(Language language) noexcept;
}
