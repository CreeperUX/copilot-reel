// Copilot Reel visual specification. English-only development reference.
// This source table is not the PDF build pipeline; build_vi.py generates the VI.
#set text(font: "Segoe UI", size: 10pt)
#set page(margin: 24mm)
= Copilot Reel / Soft Orbit and OSD Controls
Version 0.5 for application 0.2.0. All dimensions are in DIP.

#table(
  columns: (2fr, 1fr, 3fr),
  [Token], [Value], [Usage],
  [Reel diameter], [336], [Full orbit around the pointer],
  [Cancel diameter], [88], [Release here to cancel],
  [App tile], [40], [Shared monochrome vector symbol; 10 DIP tile radius],
  [App symbol], [28], [Recognizable VI mark; neutral app-window fallback],
  [Symbol stroke], [1.75], [2 DIP in high contrast; no shell bitmap or initials],
  [OSD client area], [560 x 360], [Native caption and frame; scrollable overflow],
  [Control radius], [6-10], [Rows, buttons, and fields],
  [Content inset], [24], [OSD content margin],
  [Row height], [47], [Secondary settings rows],
  [Body / hint text], [14 / 12], [Segoe UI Variable with system fallback],
  [Appearance animation], [100 ms], [Compositor opacity and scale],
  [Selection transition], [Immediate], [Future animation ceiling: 80 ms],
  [OSD page transition], [Immediate], [Future ceiling: 120 ms; Back preserves context],
)

== System appearance
The neutral surface follows the Windows app color mode. The selected item uses
the Windows accent. Disable backdrop sampling when transparency is off. High
contrast has priority over decorative materials. Reduced motion is independent.

== Navigation
Primary menu: Apps, Appearance, Settings. Language is a secondary Settings item.
Advanced launch fields belong in a separate compact sheet. Standard buttons,
list boxes, edits, and language selection retain native semantics. Settings
contains Language and About; there is no autostart preference in this release.

== Pointer placement
Keep the full circle, preserving slot order, angles, radius, and icon size. Apply
the smallest translation needed to fit the monitor work area. Do not warp the
system pointer by default. If it starts outside the relocated center zone,
keep selection empty until center entry; release before entry cancels. Once
armed, use the visual center for all hit testing. No timer changes the origin.
This implemented guard adds an inward movement at edges. A work area too small
for the complete reel opens the OSD with native overflow scrollbars. Half-orbit
and fan layouts are rejected. Usability and display acceptance remain explicit.
