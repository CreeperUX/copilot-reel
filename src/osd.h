#pragma once
#include <windows.h>

namespace reel {
// Modeless, native configuration OSD. All changes notify the controller.
void ShowOsd(HWND owner);
void RefreshOsdAppearance();
// Returns false when the user cancels the unsaved-editor confirmation.
bool CloseOsd();
}
