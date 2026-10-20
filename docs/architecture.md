# Architecture

## Platform and scope

Copilot Reel uses the official Windows Copilot hardware key provider contract and accepts its approximately 300 ms hold threshold. It does not install a keyboard driver or intercept Copilot with a global keyboard hook.

The implementation is one native C++20 process with a Win32 message loop. Windows Composition owns the visual tree and property animations; Direct2D and DirectWrite render geometry, icons, and text into reusable surfaces. The first visual scheme is Soft Orbit.

VI v0.8 defines a quiet radial interface with one combined + entry. Main contains configured agents and +; its idle center is empty. The hub contains language, appearance, preferred agent, management, and adding. EN/ZH and sun/moon convey state in the icons. Idle submenus retain a center back chevron; selected applications and commands show one concise functional name as center text. No generic page heading or instruction is drawn.

Only the wheel registers as a provider. Its targets are ordinary executables, shortcuts, URIs, or applicable packaged application AUMIDs.

## Input and lifecycle

The package declares `com.microsoft.windows.copilotkeyprovider`. Its three states share one controller:

| State | URI | MessageWParam |
| --- | --- | --- |
| SingleTap | `copilot-reel://?state=Tap` | `0` |
| PressAndHoldStart | `copilot-reel://?state=Down` | `1` |
| PressAndHoldStop | `copilot-reel://?state=Up` | `2` |

Message values are application-defined, not Windows enum values. The manifest and handler must agree. The private protocol accepts only these exact URIs and never arbitrary launch destinations.

For the fast path, `SHGetPropertyStoreForWindow` obtains the HWND property store. The key is GUID `38652BCA-4329-4E74-86F9-39CF29345EEA`, ID `2`; its `VT_UINT` value identifies the application's `WM_APP`-range Copilot message.

A named mutex and resident controller window provide single-instance behavior. Subsequent processes use a bounded `WM_COPYDATA` handoff and exit after acceptance. Handoff and provider messages enter the same state machine. Synthetic tests have passed; Shell discovery of the hidden receiver and physical-key timing remain separate checks.

| State and event | Result |
| --- | --- |
| Idle + hold start | Capture the cursor anchor, start a gesture, show |
| AwaitCenter + pointer movement | Keep selection empty until entry into the visible center |
| Armed + pointer movement | Calculate direction from the visible center; center means cancellation |
| Visible + hold stop | Resolve once, start exit, and activate the selected target once |
| Idle + hold stop | Ignore without launching |
| Duplicate hold start | Preserve the current gesture |
| Short Copilot tap | Launch the preferred local application; request a local target if unavailable |
| Explicit preview command | Show click-to-select preview |
| Release on a menu command | Enter the radial submenu and retain a deliberate adjustment context |
| Right-click or Esc | Return one menu level; cancel at Main |
| Session lock/disconnect, display/power interruption | Cancel |

Release is independent of animation completion. Gesture state owns deduplication and invalidates its generation when canceled. URI handoff packets include the producer process creation time and the pointer captured at its entry point. A release tombstone rejects a delayed older Down, including when Up initializes the resident first. Same-process HWND events use their receipt timestamp. Unit tests cover reordering; native tests cover cold Down, cold Tap, and orphan Up. The provider contract has no physical gesture sequence ID, so process timestamps cannot prove perfect ordering under every Shell scheduling race.

## Window and pointer

The held wheel is a non-activating tool popup. Opening a hold preserves keyboard focus and records one cursor anchor. Raw Input is registered only while visible; pointer messages can also update hit testing. Movement within the same sector does not trigger selection repaint. Deliberate preview and submenu navigation activate the window for keyboard adjustment; closing restores the prior foreground window where permitted. Production code never warps the pointer.

Layout and hit testing share DIP geometry and the final visible center. The process is Per-Monitor DPI aware. Capture the cursor before initialization or placement. If the complete canvas fits, place its center exactly at that pointer with no offset or cursor warping. At a boundary, apply only the minimum complete-footprint translation into the monitor work area. Arm immediately for ordinary interior opening; require center entry only if clamping leaves the pointer outside the center zone. Tiny work areas scale the full circle uniformly, including text and targets. Mixed-DPI and small-display usability remain part of the validation matrix.

Closing resolves input immediately, starts the short compositor exit, and uses a one-shot timer (the duration `BeginHide` returns) to hide the HWND and detach the visual tree. While closing, and whenever no gesture is active, the window returns `HTTRANSPARENT` and never acts on a click. Windows forwards `HTTRANSPARENT` only to windows of the same thread, so a click over the plate during the 83-133 ms exit still does not reach another application; true pass-through would need `WS_EX_LAYERED | WS_EX_TRANSPARENT`, which is not yet validated with the composition target. A new opening first finishes a pending exit, then places the window; an opening that cannot be placed leaves it hidden. Launches run on a short-lived COM STA worker, exactly once per commit, so Shell activation never holds the exit; failures return to the UI thread for the message box. Forced cancellation and reduced motion can hide immediately. The message loop remains; lifecycle timers are not frame timers.

## Responsibility boundaries

UI and logic are separated within one process. The design currently needs neither a separate UI process nor a dedicated per-frame rendering thread.

| Component | Responsibility |
| --- | --- |
| Win32 controller | Copilot/URI commands, handoff, pointer input, tray, lifecycle |
| GestureState and hit testing | Selection, cancellation, release, deduplication, angular geometry |
| AppearanceSource | Windows colors, effects, animations, high contrast, power settings |
| OrbitRenderer | Layered composition tree, Direct2D/DirectWrite surfaces, acrylic material, all motion |
| Artwork (`artwork.cpp`) | Target-based vendor identity, Shell icons, artwork classes, per-DPI raster prescale, Segoe Fluent Icons command glyphs |
| Design tokens | `design/vi/tokens.json`, generated into `src/vi_tokens.h` |
| Configuration/settings | Radial hierarchy, preferred-agent and language persistence, focused editor, validation |
| Platform activation | Shell and packaged-app activation |

The renderer consumes state and cannot decide what to launch. Appearance changes preserve anchor and selection. Configuration reload cancels a gesture before replacing targets.

## Appearance and materials

Material precedence:

1. High contrast: system solid colors, no backdrop.
2. Transparency disabled, energy saver active, or effects unavailable: opaque light/dark neutral surface.
3. Transparency enabled and supported: frosted host backdrop.

The base follows app light/dark appearance with Windows neutrals (`#F9F9F9` / `#2C2C2C`). Selection uses the live accent ramp, never the raw accent: `AccentDark1` in light and `AccentLight2` in dark for fills and strokes, `AccentDark2` / `AccentLight3` for accent text. Solid surfaces have alpha `1` and no background sampling. High contrast uses `GetSysColor` window, text, highlight, and highlighted-text colors. `COPILOT_REEL_VISUAL_OVERRIDE` (theme, material, contrast, motion, DPI) exists only for native captures and is never persisted.

`UISettings` supplies effects/animation preferences and foreground/accent colors. Its change events and energy-saver events post to the UI thread, where updates are coalesced. Win32 setting/theme/color messages also refresh the snapshot; `SPI_GETHIGHCONTRAST` checks high contrast. Exit revokes subscriptions and clears callback access to the HWND. There is no per-frame registry polling.

The frosted path (`acrylic.cpp`) follows the WinUI acrylic graph over the shell's host backdrop, which the shell has already blurred: a luminosity blend and a tint blend with the token colors, then a tiled 2% noise layer, through public Direct2D effects described with `IGraphicsEffectD2D1Interop` (no Win2D, no extra blur). The circular visual is clipped to the plate. The transparency preference permits effects but does not prove this HWND can display them; capability changes re-apply the material, and capability or error paths fall back to solids. `SetAppearance` is idempotent, so an unchanged appearance never rebuilds the material on open. Successful API setup is not visual proof of the backdrop.

## Composition tree and resources

The renderer keeps one composition tree per window, back to front:

1. Root: open/close opacity and scale around the plate center.
2. Acrylic backdrop (frosted only), then the base surface: one baked flyout shadow (Direct2D Shadow effect, knocked out under the plate), the plate fill when solid, and the inset rim.
3. Center: idle ring, gated ring and gate dot.
4. Pages: one container per orbit page. Each holds the selection seat (driven by an `Angle` property and an expression along the orbit) and one container per item with its neutral tile fill and its artwork sprite (plus a selected variant where the badge ring or high-contrast glyph color differs), and the language chevrons.
5. Handoff layer for the tile that travels to the center on drill-in.
6. Back chevron and two center-label sprites for crossfades.

Every surface has an even device-pixel size and a snapped offset, so items, tiles and hairlines land on the pixel grid at 100-250% scale; hit testing keeps exact geometry. Item artwork is rasterized once per identity, theme and DPI; selection and hover never redraw it. The idle reel produces no surface draws and no running expressions: the seat's expression stops when its travel settles, when the seat hides mid-travel, and on every reset. Updates that arrive during the exit animation are deferred until the next opening. Device loss replaces the rendering device in place and redraws everything once.

## Motion

All motion is compositor-driven with values from `tokens.motion`; interruptions start from presentation values.

| Event | Motion |
| --- | --- |
| Open | Opacity 0 to 1 in 83 ms linear; scale 0.96 to 1 in 167 ms decelerate, at the plate center |
| Cancel | Opacity to 0 in 83 ms linear; scale to 0.98, accelerate |
| Commit | Everything else fades in 83 ms; the chosen item and seat hold 50 ms with a 1 to 0.975 to 1 press, then fade (133 ms) |
| Selection | The seat fades in (83 ms, 0.94 to 1 in 167 ms), then travels the shortest arc on a spring (damping 0.85, period 50 ms) as the tile fill crossfades with it |
| Center | Name crossfade 83 ms; the ring gives way while a name shows; arming the gate relaxes the ring in 167 ms |
| Drill in / out | The outgoing page scales to 1.04 / 0.96 and fades in 100 ms; the incoming page scales from 0.94 / 1.04 in 250 ms (0.1, 0.9, 0.2, 1) and fades in after 50 ms; the plate never moves |
| Handoff | On drill-in the tapped tile travels to the center in 200 ms, scaling to 0.5, and hands over to the back chevron |
| Swap | Preset paging crossfades with a 0.98 to 1 settle in 120 ms |

Reduced motion and high contrast run the same path with 1 ms durations. There is no pointer tether, circular reveal, perimeter sweep, rotating artwork, staggered entrance or hover magnification; easing has no overshoot, and the seat spring (damping 0.85) overshoots by well under one percent.

## Design tokens

`design/vi/tokens.json` is the hand-edited source for geometry, palettes, accent roles, material, motion, typography and per-brand artwork treatment. `tools/gen-vi-tokens.ps1` validates it and writes the committed `src/vi_tokens.h`; `tools/build.ps1` and CMake run the generator before compiling, and `-Check` fails when the header is stale. The Python VI generator reads the same file, so the identity document and the application cannot drift apart silently.

There is no pre-rendered frame sequence or fixed 60 Hz application timer. The compositor evaluates animation at presentation cadence without application redraws on every refresh. This reduces work but does not guarantee frame rate or GPU cost.

Performance validation must cover 60/144/240 Hz, mixed refresh rates, different DPI, integrated graphics, and battery/energy saver. Measure first-visible latency, selection latency, frame-time variation, and static/hidden CPU/GPU activity against an application-closed baseline. See [validation](validation.md).

The [testing plan](testing-plan.md) defines the proposed separation between system-state collection, pure appearance/material policy, injected native rendering and real ETW/WPR measurements. These seams are not yet implemented: current forced high contrast still reads live system colors, and the frosted override can supersede the earlier energy-saver decision. The plan also records a proposed shared Confirm/Back/Adjust interaction contract; it does not describe current menu-open Copilot behavior as already fixed.

## Language and identity

Text uses the `Segoe UI Variable Text` family (static `Segoe UI` where it is missing) with the interface locale, and one custom DirectWrite fallback maps Han and CJK punctuation to `Microsoft YaHei UI`, so Chinese names never mix Traditional or Japanese faces within a word. `AppDisplayName` localizes recognized Notepad and Windows Settings default labels at the rendering boundary, including historical Chinese names. It does not mutate `AppTarget`, launch fields, or persisted preferences. Both languages pass the same stored target and name into icon identification; menu action labels are localized separately from their invariant `reel:` action IDs. The editor also presents built-in labels in its current language, and its initialization guard prevents presentation changes from becoming edits.

One active profile supplies apps, ordering, favorite, and language. The application and diagnostic paths come from `ConfigPath`; language does not choose a path. Windows packaging can redirect an installed app into its own LocalCache scope, independently of standalone development files.

## Configuration and activation

The native editor supports 1-8 targets. Main contains configured apps and a single +. The + opens the combined hub, ordered Language, Appearance, Preferred agent, Manage apps, and Add app. Language displays EN/ZH at the top tile with flanking 40 DIP arrow hit regions, or A/D. Appearance uses sun/moon for the current system theme and opens ms-settings:colors. The center chevron or Esc returns one level. Missing presets open the focused native editor with an empty launch target. See [configuration and isolation](configuration.md).

Short tap first resolves a usable local target. Installed-agent discovery can replace an exact legacy default website profile with the matching verified native application. Before automatically saving a migration, the existing configuration is backed up. Only known ChatGPT and Claude base-URL default profiles are eligible; arbitrary pages and custom URLs are not migrated. A missing native destination opens the native application picker, restricted to executable or shortcut files, instead of a browser. Selecting a web profile as preferred uses the same picker. A successful choice replaces that launch profile with the chosen local target. Explicit custom web targets remain available to wheel selection, but never serve as a short-tap fallback.

Shell targets use `ShellExecuteExW`; AUMIDs use `IApplicationActivationManager`. Existing instances should use each target's activation contract. Windows foreground rules apply; cold, running, and minimized states need per-application verification.

## Current milestone

Compilation, unit tests, controller-to-test-launcher integration, native captures across light/dark, solid, the high-contrast code path and 100/150/200% scale, motion frame strips, and signed development installation succeeded. Actual Windows Settings recognition and the currently selected route were confirmed on build 26200.9711. Physical-key events, pathological cross-process scheduling, the live frosted backdrop over real content, real high-contrast themes, and performance acceptance remain open. The prototype is not fully validated.

References: [Windows themes](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-windows-themes), [composition tailoring](https://learn.microsoft.com/en-us/windows/apps/develop/composition/composition-tailoring), [high contrast](https://learn.microsoft.com/en-us/windows/win32/winauto/high-contrast-parameter).
