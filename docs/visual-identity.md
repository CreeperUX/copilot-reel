# Visual identity v0.5: Unified Orbit and native OSD controls

Soft Orbit is the selected reel design. Fluent Halo and Command Petals remain comparison assets. Application v0.2.0 implements the OSD hierarchy, shared vector symbols, and guarded edge placement. The compact primary menu replaces the large configuration form; focused secondary pages retain native control semantics.

## Deliverables

- [English VI PDF](../output/pdf/copilot-reel-vi-v0.5.pdf): 12 pages covering the reel, OSD hierarchy, app editing, system appearance, edge handling, precedents, and vector handoff.
- [SVG masters](../design/vi/vectors): 26 editable vector assets.
- [Design tokens](../design/vi/tokens.json): semantic colors, geometry, timing, and implementation status.
- [Style specification table](../design/vi/stylesheet.typ): English Typst source. The PDF is built by the Python scene generator, not this Typst file.

Earlier Chinese review drafts have been archived outside the active repository. PNG snapshots are generated from the PDF for review and are not runtime UI assets.

## Navigation

The configuration OSD has a 560 x 360 DIP logical client area inside a native Windows caption and frame. It is centered within the current monitor work area. Small work areas use native overflow scrollbars, preserving readable control sizes. The launch reel remains anchored to the pointer. These are different surfaces with different placement rules.

| Level | Page | Content |
| --- | --- | --- |
| Primary | Quick menu | Apps, Appearance, Settings |
| Secondary | Apps | Familiar app names and shared vector symbols; add, reorder, edit, remove |
| Secondary | Appearance | Windows-controlled theme, material, and accent; link to Windows appearance |
| Secondary | Settings | Native language selector and About |
| Task sheet | Edit app | Display name and target; browse for a file or shortcut |
| Task sheet | Advanced | Launch method, arguments, and working directory; returns to the same draft |

Language belongs in Settings rather than the primary panel. English is the default; the native application also supports Simplified Chinese. Changing the interface language must not rename user-defined targets.

Back or Esc returns one level while preserving context. Close dismisses the OSD. The hold-to-launch gesture never opens an app editor. The primary menu does not show raw Shell types, AUMIDs, command-line arguments, or working directories.

There is no autostart preference in this release. Native buttons, list boxes, edit controls, and the language combo retain keyboard, focus, and accessibility semantics while custom drawing supplies the OSD styling.

## Appearance

Use Segoe UI Variable with Segoe UI and system CJK fallback. Every app uses the same monochrome vector treatment. The wheel and OSD share `DrawAppSymbol`: recognized targets receive the VI symbol associated with their visible identity; unknown targets receive a neutral application-window symbol. The selected symbol uses the Windows accent, or highlighted-text color in high contrast. Full-color shell bitmaps and initials are not used.

| Token | Value |
| --- | --- |
| Reel diameter | 336 DIP |
| Center cancellation diameter | 88 DIP |
| Wheel icon tile | 40 DIP, 10 DIP corner radius |
| Wheel symbol / stroke | 28 DIP / 1.75 DIP; 2 DIP stroke in high contrast |
| Icon orbit radius | 112 DIP |
| Selected item seat | 60 x 64 DIP |
| OSD panel | 560 x 360 DIP |
| OSD outer frame | Native Windows caption and corner treatment |
| OSD content inset | 24 DIP |
| Secondary row height | 47 DIP |
| App label / status hint | 14 / 12 DIP |

The neutral surface follows the Windows app light/dark mode. Selection uses the Windows accent; the entire base is not filled with a saturated accent color. Sample accents are `#005FB8` in light previews and `#60CDFF` in dark previews, not fixed runtime values.

Material priority:

1. High contrast: system background, foreground, and highlight colors; no frost.
2. Transparency disabled or effect unavailable: opaque theme surface, alpha 1, no backdrop sampling.
3. Transparency enabled and supported: one compositor backdrop layer under clear foreground content.

The material follows system changes without altering app order, pointer origin, or the current selection. Native nonactivating-window backdrop behavior remains a separate validation concern. A translucent SVG is a tonal illustration, not proof of actual desktop blur.

## Pointer-edge adaptation

The reel opens around the pointer whenever the complete footprint fits. Near an edge or corner, preserve the complete circle and move it the minimum distance needed to fit the current monitor work area. Item order, angles, radius, and icon size remain unchanged. The previous half-orbit and staggered-fan proposals are rejected.

Default input policy: do not warp the system pointer. If the initial pointer lies outside the relocated center cancellation zone, enter `AwaitCenter`: no item highlights, crossing an outer icon does not select it, and release cancels. Entering the visible center changes the state to `Armed`; subsequent selection uses that same visible center. There is no timer-driven switch of logical origin. If the initial pointer is already inside the relocated center zone, arm normally.

The cost is explicit: edge/corner activation can require an inward movement followed by the selection stroke. Pointer recentering is not implemented or enabled. Relative virtual-cursor behavior is not used.

These behaviors are implemented. Rendering and hit testing consume the same geometry and center. A work area too small to fit the wheel opens the scrollable OSD instead of squeezing or clipping the radial layout. See [edge-placement research](edge-placement-research.md) for primary sources and the distinction between observed behavior and this project's policy.

## Motion and rendering

The implemented appearance animation is 100 ms, using compositor opacity and scale. Selection and OSD page changes are immediate in v0.2.0. Future transitions may use up to 80 ms for selection and 120 ms for OSD navigation; these are design ceilings, not implemented animations. Reduced motion applies the final state immediately. Release commits the chosen target independently of animation completion.

Reuse device resources, text formats, and DPI-dependent drawing surfaces. Symbols are vector drawing commands, rendered only when content changes. Animate compositor properties rather than pre-rendering an image sequence. Stop application rendering work while hidden. Keep optional blur and shadows separate from readable vector content.

## Implementation status

The native reel, shared symbols, appearance synchronization, provider registration, app launching, OSD navigation, focused editor, and guarded full-circle edge placement are implemented. The vector OSD scenes illustrate layout and hierarchy; native frame and standard input-control rendering vary with Windows. Current verification results and remaining language, display, physical-key, material, and performance checks belong in [validation](validation.md); this VI is not a blanket test-pass claim.

## Primary references

- [Windows materials](https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/materials)
- [Windows geometry](https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/geometry)
- [Windows typography](https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/typography)
- [Windows motion](https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/motion)
- [Windows theme support](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-windows-themes)
- [Composition tailoring](https://learn.microsoft.com/en-us/windows/apps/develop/composition/composition-tailoring)
- [Apple materials](https://developer.apple.com/design/human-interface-guidelines/materials)

Reviewed on 2026-10-19. All development documentation, tokens, and comments use English. Chinese strings are retained only for the application's explicit Simplified Chinese localization and Unicode test fixtures.
