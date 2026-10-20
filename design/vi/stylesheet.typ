// Copilot Reel visual specification. English-only development reference.
// The Python scene generator produces the PDF; this table is editable source.
#set text(font: "Segoe UI", size: 10pt)
#set page(margin: 24mm)
= Copilot Reel / Quiet Soft Orbit
Version 0.7. Dimensions use DIP. Runtime validation is tracked separately.

#table(
  columns: (2fr, 1fr, 3fr),
  [Token], [Value], [Usage],
  [Reel diameter], [336], [Complete circular surface on every menu level],
  [Icon orbit], [112], [Stable app and command locations],
  [Cancel diameter], [88], [Center-entry guard only after boundary displacement],
  [App tile], [40], [Official artwork in a common container; 10 DIP radius],
  [Official artwork], [28], [Preserve vendor proportions and recognizable colors],
  [Command stroke], [1.75], [2 DIP in high contrast; internal commands only],
  [Selection seat], [60 x 64], [System accent local to selection],
  [Language icon], [EN / CN], [Active interface language inside the tile],
  [Language arrow hit], [40 x 40], [Centers at -48/+48 horizontally and -112 vertically],
  [Language arrow visual], [5 x 10], [Unfilled chevrons flanking the top language tile],
  [Appearance icon], [Sun / moon], [Reflect current Windows app light/dark mode],
  [Selected app / monogram], [14 / 16], [No generic center title, helper text, or tile captions],
  [Interior pointer offset], [0], [Mouse remains at the wheel center without cursor warping],
  [Entry animation], [160 ms], [Opacity 0 to 1; scale 0.96 to 1; no overshoot],
  [Exit animation], [120 ms], [Reverse opacity and scale, then detach the tree],
  [Selection / page change], [Immediate], [No per-frame application repaint loop],
)

== System appearance
The neutral base follows Windows app light/dark mode. Selection uses the system
accent. The perimeter remains neutral: no blue outline sweep or connector.
Transparency off disables backdrop sampling. High contrast takes priority;
reduced motion applies the final state immediately.

== Radial navigation and visual copy
Main contains configured agents and one + entry. The + opens a five-item hub:
language, appearance, preferred agent, app management, and adding. No separate
Settings gear remains on Main. Main's idle center is empty. Submenus show only a
small back chevron. A selected real app may show its name, without a second line.
Page titles, command labels, keyboard hints, and instructional center copy do not
appear inside the product UI. Semantic action names remain in the model.

== Language and appearance
EN/CN encode the active language in the top tile. Two small arrows flank that
tile; A / D offers the same adjustment. English is the default. Native text
editors preserve Chinese input methods. A sun or moon reflects current Windows
appearance; selecting it opens Windows color settings. No captions sit beneath
these controls. User app names remain unchanged.

== Copilot input and pointer placement
A short tap opens the saved preferred agent directly. A hold uses the official
Windows threshold and opens the circle centered on the captured pointer.
Only an actual boundary conflict applies the minimum inward translation needed
to keep the full canvas inside the work area. Never warp the pointer. If that
translation puts it outside the center zone, require center entry before arming
selection. Normal interior opening requires no extra center-entry movement.
The visible center is the sole hit-test origin. Tiny work areas scale the entire
circle uniformly; validate the resulting smaller controls.

== Artwork and motion
Use verified vendor files, recorded in assets/brands/provenance.json. Supported
SVGs stay vector. Official PNGs are allowed for unavailable or unsupported SVG
sources; they are identity assets, never animation frames. Animate only native
composition opacity and scale. No tether, circular reveal, perimeter sweep, or
rotating artwork remains. Interrupt closing from its current presentation value.
