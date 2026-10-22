#pragma once

#include "app_types.h"

namespace reel {
// The Add menu's catalog in src/agent_catalog.h order. Every core product appears; an
// empty target means it was not found and the Add flow must let the user choose an
// executable or shortcut before saving. Discover-only products appear only when
// installed. Win32 products are stored as their executable, never a shortcut.
// This is a catalog for the Add menu, not a replacement for the user's saved entries.
// Callable on any thread with COM initialized (STA or MTA). It shows no UI and never
// executes or resolves a target.
std::vector<AppTarget> AgentPresets();
}
