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

Closing resolves input immediately, starts the short reverse compositor animation, and uses a one-shot timer to hide the HWND and detach the visual tree. A new opening cancels pending exit completion. Forced cancellation and reduced motion can hide immediately. The message loop remains; lifecycle timers are not frame timers.

## Responsibility boundaries

UI and logic are separated within one process. The design currently needs neither a separate UI process nor a dedicated per-frame rendering thread.

| Component | Responsibility |
| --- | --- |
| Win32 controller | Copilot/URI commands, handoff, pointer input, tray, lifecycle |
| GestureState and hit testing | Selection, cancellation, release, deduplication, angular geometry |
| AppearanceSource | Windows colors, effects, animations, high contrast, power settings |
| OrbitRenderer | Direct2D/DirectWrite, composition visuals, backdrop, animation |
| Configuration/settings | Radial hierarchy, preferred-agent and language persistence, focused editor, validation |
| Platform activation | Shell and packaged-app activation |
| Shared iconography | Verified official artwork in consistent tiles; native vector command and fallback glyphs |

The renderer consumes state and cannot decide what to launch. Appearance changes preserve anchor and selection. Configuration reload cancels a gesture before replacing targets.

## Appearance and materials

Material precedence:

1. High contrast: system solid colors, no backdrop.
2. Transparency disabled, energy saver active, or effects unavailable: opaque light/dark neutral surface.
3. Transparency enabled and supported: frosted host backdrop.

The base follows app light/dark appearance; selection uses the accent. Solid surfaces have alpha `1` and no background sampling. High contrast uses `GetSysColor` window, text, highlight, and highlighted-text colors.

`UISettings` supplies effects/animation preferences and foreground/accent colors. Its change events and energy-saver events post to the UI thread, where updates are coalesced. Win32 setting/theme/color messages also refresh the snapshot; `SPI_GETHIGHCONTRAST` checks high contrast. Exit revokes subscriptions and clears callback access to the HWND. There is no per-frame registry polling.

The frosted path uses a host backdrop brush and Gaussian blur clipped to the circular base. The transparency preference permits effects but does not prove this HWND can display them. Capability/error paths fall back to solids. Successful API setup is not visual proof of correct blur.

## Animation and resources

Device resources, text formats, and surfaces are reused. Content redraws on selection, center-gate, appearance, menu, target, DPI, or device changes. The wheel uses 40 DIP tiles, 28 DIP official marks, and 1.75 DIP internal command strokes. Selection uses the system accent; official marks retain vendor proportions and colors. SVG sources use Direct2D where supported; official PNGs provide a source-aware fallback. Assets are loaded before a gesture, with device-domain caches cleared after device loss. Unknown identities get a neutral app glyph, never a guessed brand drawing.

Entry lasts 160 ms and animates only opacity from 0 to 1 and scale from 0.96 to 1. Exit reverses these properties over 120 ms, starting from the current presentation values, then detaches the tree. No pointer tether, circular reveal, perimeter sweep, or rotating artwork remains. The base outline is neutral; accent is confined to selection and preferred markers. Reduced motion and high contrast apply the final state directly. Selection and page updates are immediate.

There is no pre-rendered frame sequence or fixed 60 Hz application timer. The compositor evaluates animation at presentation cadence without application redraws on every refresh. This reduces work but does not guarantee frame rate or GPU cost.

Performance validation must cover 60/144/240 Hz, mixed refresh rates, different DPI, integrated graphics, and battery/energy saver. Measure first-visible latency, selection latency, frame-time variation, and static/hidden CPU/GPU activity against an application-closed baseline. See [validation](validation.md).

## Language and identity

`AppDisplayName` localizes recognized Notepad and Windows Settings default labels at the rendering boundary, including historical Chinese names. It does not mutate `AppTarget`, launch fields, or persisted preferences. Both languages pass the same stored target and name into icon identification; menu action labels are localized separately from their invariant `reel:` action IDs. The editor also presents built-in labels in its current language, and its initialization guard prevents presentation changes from becoming edits.

One active profile supplies apps, ordering, favorite, and language. The application and diagnostic paths come from `ConfigPath`; language does not choose a path. Windows packaging can redirect an installed app into its own LocalCache scope, independently of standalone development files.

## Configuration and activation

The native editor supports 1-8 targets. Main contains configured apps and a single +. The + opens the combined hub, ordered Language, Appearance, Preferred agent, Manage apps, and Add app. Language displays EN/ZH at the top tile with flanking 40 DIP arrow hit regions, or A/D. Appearance uses sun/moon for the current system theme and opens ms-settings:colors. The center chevron or Esc returns one level. Missing presets open the focused native editor with an empty launch target. See [configuration and isolation](configuration.md).

Short tap first resolves a usable local target. Installed-agent discovery can replace an exact legacy default website profile with the matching verified native application. Before automatically saving a migration, the existing configuration is backed up. Only known ChatGPT and Claude base-URL default profiles are eligible; arbitrary pages and custom URLs are not migrated. A missing native destination opens the native application picker, restricted to executable or shortcut files, instead of a browser. Selecting a web profile as preferred uses the same picker. A successful choice replaces that launch profile with the chosen local target. Explicit custom web targets remain available to wheel selection, but never serve as a short-tap fallback.

Shell targets use `ShellExecuteExW`; AUMIDs use `IApplicationActivationManager`. Existing instances should use each target's activation contract. Windows foreground rules apply; cold, running, and minimized states need per-application verification.

## Current milestone

Compilation, core/configuration tests, controller-to-test-launcher integration, and signed development installation succeeded. Actual Windows Settings recognition and the currently selected route were confirmed on build 26200.9711. Physical-key events, pathological cross-process scheduling, and backdrop/performance acceptance remain open. The prototype is not fully validated.

References: [Windows themes](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-windows-themes), [composition tailoring](https://learn.microsoft.com/en-us/windows/apps/develop/composition/composition-tailoring), [high contrast](https://learn.microsoft.com/en-us/windows/win32/winauto/high-contrast-parameter).
