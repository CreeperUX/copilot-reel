// Copilot Reel visual specification. English-only development reference.
// The Python scene generator produces the PDF; this table is editable source.
#set text(font: "Segoe UI", size: 10pt)
#set page(margin: 24mm)
= Copilot Reel / Soft Orbit and Radial OSD
Version 0.6. Dimensions use DIP. Runtime validation is tracked separately.

#table(
  columns: (2fr, 1fr, 3fr),
  [Token], [Value], [Usage],
  [Reel diameter], [336], [Complete circular surface on every menu level],
  [Icon orbit], [112], [Stable app and command locations],
  [Cancel diameter], [88], [Center-entry guard and cancellation],
  [App tile], [40], [Official artwork in a common container; 10 DIP radius],
  [Official artwork], [28], [Preserve vendor proportions and recognizable colors],
  [Command stroke], [1.75], [2 DIP in high contrast; internal commands only],
  [Selection seat], [60 x 64], [System accent outline and indicator],
  [Language arrow hit], [44 x 44], [Centers at -60 and +60 DIP horizontally],
  [Language arrow visual], [30 x 30], [Small chevrons beside the current value],
  [Body / hint text], [14 / 12], [Segoe UI Variable with system fallback],
  [Nominal tether], [168], [Pointer to wheel center; edge clamping can extend it],
  [Entry animation], [230 ms], [70 ms tether followed by 160 ms reveal],
  [Exit animation], [190 ms], [Reverse reveal, retract tether, detach tree],
  [Selection / page change], [Immediate], [No per-frame application repaint loop],
)

== System appearance
The neutral base follows Windows app light/dark mode. Selection uses the system
accent. Transparency off disables backdrop sampling. High contrast has priority
over decorative material. Reduced motion applies the final state immediately.

== Radial navigation
The main wheel contains configured agents, + app management, and Settings.
Settings is another wheel: Language, Preferred agent, and Windows appearance.
Language arrows stay visible in Settings; left/right chevrons or A / D adjust it. English is the
default; Simplified Chinese is a native interface option. User app names stay
unchanged. Manage and preset selection remain radial. A focused native editor
is reserved for entering a name or actual launch target, including missing apps.

== Copilot input
A short tap opens the saved preferred agent directly. A hold uses the official
Windows threshold and opens the reel. A release resolves at most once. Entering
a submenu creates a persistent adjustment context; Back or Esc returns one level.

== Pointer placement
The original pointer anchors the opening line. Place the reel on the pointer's
monitor, normally one outer radius away, then clamp the complete footprint to
its work area. Preserve slots, angles, radius, and icon size. Corners can require
a longer line. Never warp the pointer. If it starts outside the final center zone,
keep selection empty until center entry; release before entry cancels. Use that
visible center for all armed hit testing. If the canvas cannot fit, scale the whole circle uniformly. This can reduce
text and target sizes; validate small-display usability rather than claiming
unchanged physical sizes. The primary interface remains radial.

== Artwork and motion
Use verified vendor files, recorded in assets/brands/provenance.json. Supported
SVGs stay vector. Official PNGs are allowed when no usable SVG exists or a feature
is unsupported; they are application identity assets, never animation frames.
Animate composition opacity, scale, clip radius, and outline trim. Keep icons
upright. Reverse the reveal on close and stop all hidden composition work.
