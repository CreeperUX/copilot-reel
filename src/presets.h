#pragma once

#include "app_types.h"

namespace reel {
// Returns installed launch targets when found. An empty target means the Add
// flow must let the user choose an executable or shortcut before saving.
// This is a catalog for the Add menu, not a replacement for the user's saved entries.
std::vector<AppTarget> AgentPresets();
}
