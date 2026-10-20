#pragma once
#include "app_types.h"
#include <d2d1.h>

namespace reel {
enum class SystemIconKind { None, Settings, Notepad };
// Recognize actual system launch destinations, independently of localized names.
SystemIconKind IdentifySystemIcon(const AppTarget& app);
// Returns false for non-system targets; the caller can keep its generic fallback.
bool DrawSystemIcon(ID2D1RenderTarget* target, const AppTarget& app,
                    const D2D1_RECT_F& bounds, ID2D1Brush* brush,
                    float strokeWidth = 1.75f);
// Theme-aware system or neutral application glyph. Vendor identities belong to
// DrawOfficialIcon and are never approximated by invented geometric marks.
void DrawAppSymbol(ID2D1RenderTarget* target, const AppTarget& app,
                   const D2D1_RECT_F& bounds, ID2D1Brush* brush,
                   float strokeWidth = 1.75f);
}
