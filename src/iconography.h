#pragma once
#include "app_types.h"
#include <d2d1.h>
#include <string>

namespace reel {
enum class SystemIconKind { None, Settings, Notepad };
// Recognize actual system launch destinations, independently of localized names.
SystemIconKind IdentifySystemIcon(const AppTarget& app);
// Vendor artwork family ("chatgpt", "claude", "pycharm", ...) or empty, decided by
// the launch target: AUMID package family, exe/lnk file name plus its install
// folder, or URL host. The display name counts only when the target is empty.
std::wstring IdentifyBrand(const AppTarget& app);
// Neutral line glyph for the native editor list: the Settings gear, the Notepad
// page, or a generic window. Never a vendor mark; the reel uses artwork.h.
void DrawAppSymbol(ID2D1RenderTarget* target, const AppTarget& app,
                   const D2D1_RECT_F& bounds, ID2D1Brush* brush,
                   float strokeWidth = 1.75f);
}
