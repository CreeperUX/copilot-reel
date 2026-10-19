#pragma once
#include "app_types.h"

namespace reel {
inline constexpr UINT kTargetEditorClosedMessage = WM_APP + 12;
// Modeless, native configuration OSD. All changes notify the controller.
void ShowOsd(HWND owner);
// Opens only the focused text/path editor. -1 adds a blank entry; an existing
// index edits that entry. Saving or closing returns control to the radial menu.
void ShowTargetEditor(HWND owner, int index);
// Opens a focused addition with the preset's fields. The launch target may be
// empty until the user supplies the installed application's path or identity.
void ShowPresetTargetEditor(HWND owner, const AppTarget& preset);
void RefreshOsdAppearance();
// Returns false when the user cancels the unsaved-editor confirmation.
bool CloseOsd();
}
