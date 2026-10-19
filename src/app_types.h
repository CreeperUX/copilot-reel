#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace reel {
inline constexpr wchar_t kWindowClass[] = L"CopilotReel.Controller";
inline constexpr wchar_t kAppName[] = L"Copilot Reel";
inline constexpr UINT kCopilotMessage = WM_APP + 1;
inline constexpr UINT kAppearanceMessage = WM_APP + 2;
inline constexpr UINT kConfigChangedMessage = WM_APP + 3;
inline constexpr UINT kTrayMessage = WM_APP + 4;
inline constexpr UINT kLanguageChangedMessage = WM_APP + 6;
inline constexpr float kCanvasDip = 384.0f;
inline constexpr float kCenterDip = kCanvasDip / 2.0f;
inline constexpr float kRadiusDip = 168.0f;
inline constexpr float kDeadZoneDip = 44.0f;

enum class TargetKind { Shell, Aumid };
struct AppTarget {
    std::wstring name;
    TargetKind kind = TargetKind::Shell;
    std::wstring target;
    std::wstring arguments;
    std::wstring workingDirectory;
};

struct Rgba { float r = 0, g = 0, b = 0, a = 1; };
struct Appearance {
    bool dark = false;
    bool transparency = false;
    bool animations = true;
    bool highContrast = false;
    Rgba surface{.957f,.965f,.973f,1};
    Rgba foreground{.125f,.145f,.169f,1};
    Rgba secondary{.384f,.42f,.463f,1};
    Rgba outline{.80f,.827f,.863f,1};
    Rgba accent{0,.373f,.722f,1};
    Rgba selection{.87f,.925f,.98f,1};
    Rgba selectionText{.125f,.145f,.169f,1};
};
}
