# Visual identity v0.8: Quiet Soft Orbit

Soft Orbit uses a restrained native surface, original application marks, and a single `+` entry for both app management and settings. The interface conveys language and appearance through the icons themselves. This specification describes implementation intent; runtime evidence belongs in [validation](validation.md).

## Deliverables

- [English VI PDF](../output/pdf/copilot-reel-vi-v0.8.pdf): 12 reviewed pages.
- [SVG masters](../design/vi/vectors): Main, unified hub, language, preferred-agent, management, and presets in light/dark variants.
- [Design tokens](../design/vi/tokens.json): geometry, timing, input, materials, and artwork policy.
- [Style specification](../design/vi/stylesheet.typ): editable English Typst table; the Python scene generator produces the PDF.
- [Official artwork provenance](../assets/brands/provenance.json): sources, ownership, processing, and hashes.

PDF and PNG previews are review artifacts, never application UI or animation frames. Native UI geometry is vector based. Supported official SVGs use Direct2D; original PNG identities remain valid when vector sources are missing or unsupported.

## Center and visual copy

Main has an empty idle center. Idle submenus show a small neutral back chevron. Hovering an application or command shows one concise functional name at the center, such as Language, Appearance, Preferred agent, Manage apps, or Add app. The label replaces the back chevron while an item is selected. It uses 14 DIP text in a 132 DIP single-line region, with ellipsis for unusually long user names. No generic page heading, second-line explanation, Choose an app prompt, release instruction, A/D hint, or Move here instruction is drawn.

Semantic action names remain in the model and development documentation. The minimal visual surface does not establish complete accessibility support; keyboard discoverability and assistive-technology semantics require their own validation.

## One combined entry

| Level | Contents |
| --- | --- |
| Main | Configured applications and a single `+` |
| Unified hub | Language, appearance, preferred agent, app management, adding |
| Preferred agent | Configured apps and the saved preferred marker |
| App management | Existing apps and Add |
| Presets | Five official app identities per page, Custom, Previous/Next |
| App actions | Edit, set preferred, remove when allowed |
| Focused native editor | Name, actual launch target, and optional launch details |

A short Copilot tap opens the preferred local application. Defaults favor installed software; recognized legacy website defaults resolve to a verified native target. If that target is unavailable, show a native application picker for an executable or shortcut rather than opening a website. Missing presets still use the focused editor. Explicit web entries remain a deliberate wheel action.

The hub order is stable: language at twelve o'clock, then appearance, preferred agent, management, and adding clockwise. The separate Main Settings gear is removed. The center or Esc returns one level; the menu remains open for deliberate adjustment after the initial hold ends.

## State in the icon

The language tile displays `EN` or `ZH`. Small chevrons flank this tile at local canvas coordinates `(144, 80)` and `(240, 80)`. The visible chevrons are 5 x 10 DIP strokes without filled backgrounds; their hit regions are 40 x 40 DIP. A/D supplies the same language adjustment. English is the default; Simplified Chinese is a native option. The update applies after persistence succeeds and never renames user app data.

Language is a presentation state over one application profile. The same raw targets and artwork feed both locales. Built-in system names use localized display labels, while custom names and saved data remain intact. An idle Main render must be identical in English and Chinese; only the EN/ZH tile changes in the idle hub.

Appearance uses a sun for the system light theme and a moon for the system dark theme. Activating it opens Windows color settings. It does not introduce a separate application theme override. Neither language nor appearance has a caption beneath its tile. Hovering either tile shows its functional name at the center. The language-arrow hit regions also select Language, preserving that label while the pointer moves to an arrow.

## Geometry and official artwork

| Token | Value |
| --- | --- |
| Canvas / visible circle | 384 / 336 DIP |
| Icon orbit radius | 112 DIP |
| Center cancellation zone | 88 DIP diameter |
| Tile / corner radius | 40 / 10 DIP |
| Official artwork bounds | 28 DIP |
| Internal command stroke | 1.75 DIP; 2 DIP in high contrast |
| Selection seat | 60 x 64 DIP |
| Hover name / language monogram | 14 / 16 DIP |
| Interior pointer offset | 0 DIP |
| Opening / closing | 160 / 120 ms |

Use official marks with their original proportions and recognizable colors. The launcher standardizes the container, optical bounds, spacing, selection, and preferred marker. Unknown identities use a neutral app glyph. EN/ZH and sun/moon are native status glyphs, not replacements for vendor branding.

The system accent is local to selected tiles and the preferred marker. The base outline is neutral. No blue perimeter sweep, decorative connector, rotating mark, or circular reveal remains.

## Pointer placement

Capture the actual pointer at invocation. If the complete canvas fits around it, its location is the final wheel center. Opening does not move the pointer and does not move the wheel one radius away.

At a boundary, apply only the minimum inward translation needed to fit the canvas inside the current monitor work area. Preserve slot order, angles, and sizes. If that displacement leaves the pointer outside the final center zone, keep selection unarmed until the pointer enters the center; early release cancels. The normal interior path requires no extra center-entry movement. Hit testing always uses the final visible center.

Tiny work areas uniformly scale the whole circle. This preserves proportions but reduces text and target sizes, so small-display usability remains an explicit test.

## Motion and materials

Opening is a 160 ms opacity transition from 0 to 1 with scale from 0.96 to 1. Closing reverses opacity and scale over 120 ms, then detaches the composition tree. Use smooth easing without overshoot. Interruption starts from the current compositor presentation value; a pending close must not hide a new gesture. Reduced motion and high contrast apply the final state immediately.

Windows Composition drives these properties without a fixed application frame timer or pre-rendered frame sequence. Content is cached and redraws only for relevant state changes. This architecture does not by itself prove 144/240 Hz smoothness or low GPU cost.

The neutral base follows Windows app light/dark mode. Transparency off means an opaque surface without backdrop sampling. Transparency on permits a frosted backdrop where supported. High contrast, effects availability, and energy saver take precedence. Frosted PDF scenes illustrate tone; actual background blur requires runtime validation.

## References

- [Windows motion](https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/motion)
- [Composition animation](https://learn.microsoft.com/en-us/windows/uwp/composition/composition-animation)
- [Windows materials](https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/materials)
- [Kando pointer behavior](https://kando.menu/config-files/#warpmouse)
- [Blender pie menus](https://docs.blender.org/manual/en/latest/editors/preferences/interface.html)

Specification date: 2026-10-20. Development documentation, comments, tokens, and sources use English. Product localization remains separate.
