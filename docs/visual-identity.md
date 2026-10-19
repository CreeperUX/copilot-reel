# Visual identity v0.4: Stable Orbit and OSD controls

Soft Orbit is the selected reel design. Fluent Halo and Command Petals remain comparison assets. The OSD extension replaces the large configuration form as the target interface, with a compact primary menu and focused secondary pages.

## Deliverables

- [English VI PDF](../output/pdf/copilot-reel-vi-v0.4.pdf): 12 pages covering the reel, OSD hierarchy, app editing, system appearance, revised edge handling, precedents, and vector handoff.
- [SVG masters](../design/vi/vectors): 26 editable vector assets.
- [Design tokens](../design/vi/tokens.json): semantic colors, geometry, timing, and implementation status.
- [Style specification table](../design/vi/stylesheet.typ): English Typst source. The PDF is built by the Python scene generator, not this Typst file.

Earlier Chinese review drafts have been archived outside the active repository. PNG snapshots are generated from the PDF for review and are not runtime UI assets.

## Navigation

The configuration OSD is a compact 560 x 360 DIP panel centered within the current monitor work area. The launch reel remains anchored to the pointer. These are different surfaces with different placement rules.

| Level | Page | Content |
| --- | --- | --- |
| Primary | Quick menu | Apps, Appearance, Settings |
| Secondary | Apps | Familiar app names and icons; add, reorder, edit, remove |
| Secondary | Appearance | Windows-controlled theme, material, and accent; link to Windows appearance |
| Secondary | Settings | Language, proposed startup preference, About |
| Task sheet | Edit app | Display name and target picker; Advanced collapsed by default |

Language belongs in Settings rather than the primary panel. English is the default; the native application also supports Simplified Chinese. Changing the interface language must not rename user-defined targets.

Back or Esc returns one level while preserving context. Close dismisses the OSD. The hold-to-launch gesture never opens an app editor. The primary menu does not show raw Shell types, AUMIDs, command-line arguments, or working directories.

The startup preference shown in the OSD is a design proposal. It must not silently enable autostart.

## Appearance

Use Segoe UI Variable with Segoe UI and system CJK fallback. Retain the original app icons at runtime; the neutral symbols in the VI are placeholders.

| Token | Proposed value |
| --- | --- |
| Reel diameter | 336 DIP |
| Center cancellation diameter | 88 DIP |
| App icon | 40 DIP |
| Icon orbit radius | 112 DIP |
| Selected item seat | 60 x 64 DIP |
| OSD panel | 560 x 360 DIP |
| OSD outer radius | 16 DIP |
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

The cost is explicit: edge/corner activation can require an inward movement followed by the selection stroke. Pointer recentering can be considered as a separately disclosed, opt-in alternative; it is not enabled by this design revision. Relative virtual-cursor behavior is not the default.

These behaviors are specifications, not yet implemented. Rendering and hit testing must consume the same geometry and center. Very small work areas require a separately evaluated compact fallback, rather than silently squeezing the layout. See [edge-placement research](edge-placement-research.md) for primary sources and the distinction between observed behavior and this project's proposal.

## Motion and rendering

Proposed durations are 100 ms for reel appearance, 80 ms for selection, and at most 120 ms for OSD page transitions. Reduced motion applies the final state immediately. Release commits the chosen target independently of any exit animation.

Cache geometry, text layouts, icons, and DPI-dependent drawing surfaces. Animate compositor properties rather than pre-rendering an image sequence. Stop application rendering work while hidden. Keep optional blur and shadows separate from readable vector content.

## Implementation status

The native reel, appearance synchronization, provider registration, app launching, and functional settings editor exist. The OSD navigation and guarded full-circle edge placement in this revision are the next UI implementation specification. The functional editor remains available during that transition.

## Primary references

- [Windows materials](https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/materials)
- [Windows geometry](https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/geometry)
- [Windows typography](https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/typography)
- [Windows motion](https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/motion)
- [Windows theme support](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-windows-themes)
- [Composition tailoring](https://learn.microsoft.com/en-us/windows/apps/develop/composition/composition-tailoring)
- [Apple materials](https://developer.apple.com/design/human-interface-guidelines/materials)

Reviewed on 2026-10-19. All development documentation, tokens, and comments use English. Chinese strings are retained only for the application's explicit Simplified Chinese localization and Unicode test fixtures.
