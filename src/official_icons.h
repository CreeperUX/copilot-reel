#pragma once

#include <windows.h>
#include <string_view>
#include <vector>

namespace reel {
// Call on the UI/COM thread during initialization, before opening the wheel.
// Reads every vendor PNG listed in vi::kBrands from assets\brands next to the
// executable; later artwork preparation and drawing perform no file I/O.
void PreloadOfficialIconAssets();

// Encoded PNG bytes of a preloaded vendor asset (file stem such as "claude" or
// "chatgpt-dark"), or nullptr when it is unknown, missing or not yet loaded.
const std::vector<BYTE>* OfficialArtworkBytes(std::wstring_view key);
}
