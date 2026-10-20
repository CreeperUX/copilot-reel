#pragma once
#include <windows.h>
#include <winrt/Windows.UI.Composition.h>
#include <vector>

namespace reel {
// Fluent acrylic over the shell's host backdrop, which the shell has already blurred:
// a luminosity blend and a tint blend over the backdrop, then a tiled noise layer.
// Public D2D effects through IGraphicsEffectD2D1Interop; no Win2D. Throws on failure.
winrt::Windows::UI::Composition::CompositionEffectBrush CreateAcrylicBrush(
    winrt::Windows::UI::Composition::Compositor const& compositor, bool dark,
    winrt::Windows::UI::Composition::CompositionBrush const& noise);

// vi::kAcrylicNoiseSize squared premultiplied BGRA pixels at vi::kAcrylicNoiseOpacity.
std::vector<BYTE> AcrylicNoisePixels();
}
