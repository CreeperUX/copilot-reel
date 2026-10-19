#pragma once
#include "app_types.h"

namespace reel {
// A versioned identity for a launch profile. Display names and list positions
// are deliberately excluded so renaming and reordering retain the preference.
std::wstring StableTargetKey(const AppTarget& target);

// Returns -1 for an empty list, otherwise the saved target or the first entry.
int PreferredTargetIndex(const std::vector<AppTarget>& targets);

// Atomically persists the preferred launch profile beside apps.ini.
bool SavePreferredTarget(const AppTarget& target);
}
