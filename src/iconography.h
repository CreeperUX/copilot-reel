#pragma once
#include "app_types.h"
#include <d2d1.h>

namespace reel {
// A single vector treatment for the wheel and OSD. Source artwork is never a
// screen-resolution screenshot; the caller controls theme color and stroke.
void DrawAppSymbol(ID2D1RenderTarget* target, const AppTarget& app,
                   const D2D1_RECT_F& bounds, ID2D1Brush* brush,
                   float strokeWidth = 1.75f);
}
