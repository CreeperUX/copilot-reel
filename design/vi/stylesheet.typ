// Copilot Reel visual specification. English-only development reference.
// This source table is not the PDF build pipeline; build_vi.py generates the VI.
#set text(font: "Segoe UI", size: 10pt)
#set page(margin: 24mm)
= Copilot Reel / Soft Orbit and OSD Controls
Version 0.4. All dimensions are design proposals in DIP.

#table(
  columns: (2fr, 1fr, 3fr),
  [Token], [Value], [Usage],
  [Reel diameter], [336], [Full orbit around the pointer],
  [Cancel diameter], [88], [Release here to cancel],
  [App icon], [40], [Preserve original app artwork],
  [OSD panel], [560 x 360], [Centered configuration surface],
  [Outer corner radius], [16], [OSD container],
  [Control radius], [6-10], [Rows, buttons, and fields],
  [Content inset], [24], [OSD content margin],
  [Row height], [47], [Secondary settings rows],
  [Body / hint text], [14 / 12], [Segoe UI Variable with system fallback],
  [Appearance animation], [100 ms], [Compositor opacity and scale],
  [Selection animation], [80 ms], [Immediate logical selection],
  [OSD page transition], [120 ms max], [Back navigation preserves context],
)

== System appearance
The neutral surface follows the Windows app color mode. The selected item uses
the Windows accent. Disable backdrop sampling when transparency is off. High
contrast has priority over decorative materials. Reduced motion is independent.

== Navigation
Primary menu: Apps, Appearance, Settings. Language is a secondary Settings item.
Advanced launch fields belong in the app task sheet, collapsed by default.

== Pointer placement
Keep the full circle, preserving slot order, angles, radius, and icon size. Apply
the smallest translation needed to fit the monitor work area. Do not warp the
system pointer by default. If it starts outside the relocated center zone,
keep selection empty until center entry; release before entry cancels. Once
armed, use the visual center for all hit testing. No timer changes the origin.
This proposed guard adds an inward movement at edges and still requires native
implementation and usability validation. Half-orbit and fan layouts are rejected.
