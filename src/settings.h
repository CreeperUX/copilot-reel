#pragma once
#include "app_types.h"

namespace reel {
std::wstring ConfigPath();
std::vector<AppTarget> LoadTargets(std::wstring& error);
bool SaveTargets(const std::vector<AppTarget>& targets, std::wstring& error);
// Modeless native window. Saving applications posts kConfigChangedMessage;
// changing the UI language saves its preference and posts kLanguageChangedMessage.
void ShowSettings(HWND owner);
}
