#pragma once

#include "app_types.h"
#include <d2d1.h>

namespace reel {
// Call on the UI/COM thread during initialization, before opening the wheel.
// All source files and WIC pixels are cached here; drawing performs no file I/O.
void PreloadOfficialIconAssets();

// Uses verified vendor artwork with its original proportions and colors.
// Supported SVGs remain vectors; vendor PNGs and unsupported SVG features use WIC.
// Returns false for missing/unknown artwork so the caller can draw a generic icon.
bool DrawOfficialIcon(ID2D1RenderTarget* target, const AppTarget& app,
                      const D2D1_RECT_F& bounds, bool dark = false);

// Device-context caches are keyed by the canonical D2D device identity.
// Legacy targets are keyed by their canonical COM identity. nullptr clears all.
void ReleaseOfficialIcons(ID2D1RenderTarget* target = nullptr);
}
