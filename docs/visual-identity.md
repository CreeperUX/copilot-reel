# Visual identity v0.6: Soft Orbit and radial OSD

Soft Orbit remains the selected visual direction. Every primary navigation level is now a wheel. The previous rectangular quick-menu design is superseded; a focused native task editor remains appropriate only for entering text or launch details. The specification documents implementation intent and does not substitute for runtime test evidence.

## Deliverables

- [English VI PDF](../output/pdf/copilot-reel-vi-v0.6.pdf): 12 pages.
- [SVG masters](../design/vi/vectors): radial main, Settings, language, preferred-agent, management, and preset scenes in light and dark themes; material variants and historical comparison assets.
- [Design tokens](../design/vi/tokens.json): geometry, timing, input behavior, artwork policy, and navigation.
- [Style specification](../design/vi/stylesheet.typ): editable English Typst table. The Python scene generator builds the PDF.
- [Official artwork provenance](../assets/brands/provenance.json): vendor files, origins, processing, and hashes.

PNG snapshots are generated from the PDF for review. They are never runtime UI or animation frames. SVG scenes preserve native vector UI geometry and embed verified source artwork; official PNG identities are retained where no usable SVG exists.

## Radial hierarchy

| Level | Wheel | Contents |
| --- | --- | --- |
| Primary | Main | Configured agents, + app management, Settings |
| Secondary | Settings | Language, Preferred agent, Windows appearance |
| Adjustment | Language | Current value at the center, small left/right chevrons, A / D |
| Selection | Preferred agent | Configured applications; current preferred marker; center returns |
| Secondary | Manage apps | Existing targets and Add; center returns |
| Selection | Presets | Five verified agent identities per page, Custom, Previous/Next |
| Action | Existing app | Edit details, preference/action options, removal, Back |
| Focused task | Native editor | Name and actual executable, shortcut, URI, or AUMID; optional launch fields |

The center or Esc restores the prior radial context. Windows appearance opens the Windows color settings page. Language arrows remain available in the Settings center, including during pointer movement. Secondary menus remain open for deliberate adjustment after the original hold ends. English is the default and Simplified Chinese is a native option. Changing language must never rename user data. A preset without a valid discovered installation opens the focused editor; do not silently replace a desktop application with an unrelated website.

A short Copilot tap directly opens the user's saved preferred agent. A hold uses the official Windows threshold, approximately 300 ms, and opens the main reel. Explicit preview commands retain a click-to-select path. Each release resolves at most one action.

## Official identities and common styling

Use official marks with their original proportions and recognizable colors. The common treatment belongs to the tile, optical bounds, spacing, selected outline, focus, and preferred marker. Do not redraw a brand into a guessed geometric approximation, recolor it to force a monochrome set, or replace it with an initial.

Initial identities include ChatGPT/Codex, Claude Desktop, Claude for Science, Antigravity, ZCode, DeepSeek Harness (`dsh`), VS Code, and JetBrains products including Junie, Air, Toolbox, and IDEs. Claude for Science shares Claude artwork; the installed OpenAI package supplies the current ChatGPT/Codex identity.

| Token | Value |
| --- | --- |
| Reel diameter | 336 DIP |
| Icon orbit radius | 112 DIP |
| Center cancellation zone | 88 DIP diameter |
| Tile / radius | 40 DIP / 10 DIP |
| Official mark bounds | 28 DIP |
| Internal command stroke | 1.75 DIP; 2 DIP in high contrast |
| Selected seat | 60 x 64 DIP |
| Language arrow visual / hit | 30 x 30 / 44 x 44 DIP |
| Language arrow centers | -60 and +60 DIP horizontally from center |
| Label / hint | 14 / 12 DIP |

Supported vendor SVGs use Direct2D. Official PNGs are allowed when no usable SVG exists or an SVG feature such as masking is unsupported. This exception concerns source artwork, not the application surface or animation pipeline. Unknown or missing artwork receives a neutral application glyph.

## System appearance

The neutral base follows Windows app light/dark mode. The system accent colors selection outlines and indicators rather than the full base. High contrast uses system surfaces and readable outlines. Transparency off creates an opaque neutral surface with no backdrop sampling; transparency on permits a frosted host backdrop when supported. Effects availability and energy saver may require an opaque fallback. Reduced motion is independent and skips decorative transitions.

Theme changes preserve app order, menu context, and selection. The VI's translucent treatment is a tonal illustration, not proof that a nonactivating HWND can display the intended blur.

## Pointer and screen edges

Capture the actual pointer position at gesture start. Select its monitor and place the complete wheel nearby, normally one outer radius (168 DIP) from the pointer. The opening line makes that spatial relation visible. Do not use a fixed screen center or a stale window position as the anchor.

Clamp the complete footprint inside the monitor work area. At an edge or corner, the line may exceed 168 DIP to keep the wheel visible. Preserve slot order, angles, orbit radius, and icon sizes. Do not warp the pointer or reshape the menu into a fan.

If the original pointer lies outside the final center zone, enter `AwaitCenter`: highlight nothing, crossing an outer icon does not select it, and early release cancels. Entering the visible center arms selection. Thereafter the visible center is the only hit-test origin. No timer changes the origin. A work area too small for the canvas scales the complete circle uniformly. This preserves proportions but reduces text and target sizes; small-display usability needs explicit validation.

## Motion and resource use

Opening lasts 230 ms: extend the pointer tether for 70 ms, then reveal the wheel over 160 ms. Windows Composition animates opacity, scale from 0.85 to 1, circular clip radius, and an outline sweep through one full circle. App icons stay upright. Closing reverses the reveal and retracts the tether over 190 ms, then detaches the visible composition tree. A new opening cancels pending exit completion. Reduced motion and high contrast apply the final state immediately.

Cache official artwork before a gesture, retain device-domain resources, and redraw surfaces only for relevant state changes. Pointer events within the same selected sector do not repaint. Animation uses compositor properties, not a fixed frame timer or pre-rendered images. At 60, 144, and 240 Hz, presentation cadence changes while animation duration remains the same.

Physical key timing, interruption behavior, mixed DPI, high refresh rates, real background blur, and assistive-technology behavior require runtime validation. Current results belong in [validation](validation.md).

## References

- [Windows motion](https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/motion)
- [Composition animation](https://learn.microsoft.com/en-us/windows/uwp/composition/composition-animation)
- [Windows materials](https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/materials)
- [Windows typography](https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/typography)
- [Kando pointer warping](https://kando.menu/config-files/#warpmouse)
- [Blender interface and pie menus](https://docs.blender.org/manual/en/latest/editors/preferences/interface.html)
- [Prior edge-placement research](edge-placement-research.md)

Specification date: 2026-10-19. Development documentation, comments, tokens, and sources use English. Chinese product strings are explicit localization resources.
