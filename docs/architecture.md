# Architecture

## Platform and scope

Copilot Reel uses the official Windows Copilot hardware key provider contract and accepts its approximately 300 ms hold threshold. It does not install a keyboard driver or intercept Copilot with a global keyboard hook.

The implementation is one native C++20 process with a Win32 message loop. Windows Composition owns the visual tree and property animations; Direct2D and DirectWrite render geometry, icons, and text into reusable surfaces. The first visual scheme is Soft Orbit.

VI v0.6 defines nested radial navigation. The main wheel contains configured agents, + app management, and Settings; secondary controls remain radial. The pointer starts a tether to a nearby wheel center, normally 168 DIP away. Placement preserves the full circle and clamps its footprint inside the pointer monitor's work area. Center entry guards release-to-launch whenever the pointer starts outside the final cancellation zone. Rectangular primary OSD, half-orbit, and fan proposals are superseded.

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
| Short Copilot tap | Launch the saved preferred agent directly |
| Explicit preview command | Show click-to-select preview |
| Release on a menu command | Enter the radial submenu and retain a deliberate adjustment context |
| Right-click or Esc | Return one menu level; cancel at Main |
| Session lock/disconnect, display/power interruption | Cancel |

Release is independent of animation completion. Gesture state owns deduplication and invalidates its generation when canceled. URI handoff packets include the producer process creation time and the pointer captured at its entry point. A release tombstone rejects a delayed older Down, including when Up initializes the resident first. Same-process HWND events use their receipt timestamp. Unit tests cover reordering; native tests cover cold Down, cold Tap, and orphan Up. The provider contract has no physical gesture sequence ID, so process timestamps cannot prove perfect ordering under every Shell scheduling race.

## Window and pointer

The held wheel is a non-activating tool popup. Opening a hold preserves keyboard focus and records one cursor anchor. Raw Input is registered only while visible; pointer messages can also update hit testing. Movement within the same sector does not trigger selection repaint. Deliberate preview and submenu navigation activate the window for keyboard adjustment; closing restores the prior foreground window where permitted. Production code never warps the pointer.

Layout and hit testing share DIP geometry and the final visible center. The process is Per-Monitor DPI aware. Capture the actual cursor position before placing the window; never derive a new anchor from the prior window position or a monitor midpoint. The nominal pointer-to-center offset is one outer radius. A pointer outside the work area, such as over a taskbar, can place the tether origin beyond the fixed canvas; that off-canvas portion is clipped while the wheel remains visible. Clamp the entire footprint to the monitor work area; edge and corner tethers may exceed that nominal distance. Do not warp the pointer. Work areas too small for the complete canvas scale the entire circle uniformly; text and hit regions shrink together and require explicit usability validation. Mixed-DPI transitions remain part of the validation matrix.

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

Entry lasts 230 ms: a 70 ms pointer tether followed by 160 ms of reveal. Windows Composition animates line scale, opacity, wheel scale, ellipse clip radius, and outline TrimEnd through one complete circle. Icons remain upright. Exit reverses the reveal and retracts the line over 190 ms, then detaches the composition tree. Reduced motion and high contrast set the final state directly. Selection and page updates are immediate.

There is no pre-rendered frame sequence or fixed 60 Hz application timer. The compositor evaluates animation at presentation cadence without application redraws on every refresh. This reduces work but does not guarantee frame rate or GPU cost.

Performance validation must cover 60/144/240 Hz, mixed refresh rates, different DPI, integrated graphics, and battery/energy saver. Measure first-visible latency, selection latency, frame-time variation, and static/hidden CPU/GPU activity against an application-closed baseline. See [validation](validation.md).

## Configuration and activation

The native editor supports 1-8 targets. Main contains configured apps, + management, and Settings. Manage and presets are wheels; Settings contains Language, Preferred agent, and Windows appearance. The latter opens ms-settings:colors. The center or Esc returns one level. Language adjusts in place with small left/right arrows or A / D. Preferred-agent selection persists the target used by a short Copilot tap. Text entry uses a focused native task sheet; a missing preset opens that editor rather than inventing a launch destination. App data stores names, types, identifiers, arguments, and working directories. See [configuration and isolation](configuration.md).

Shell targets use `ShellExecuteExW`; AUMIDs use `IApplicationActivationManager`. Existing instances should use each target's activation contract. Windows foreground rules apply; cold, running, and minimized states need per-application verification.

## Current milestone

Compilation, core/configuration tests, controller-to-test-launcher integration, and signed development installation succeeded. Actual Windows Settings recognition and the currently selected route were confirmed on build 26200.9711. Physical-key events, pathological cross-process scheduling, and backdrop/performance acceptance remain open. The prototype is not fully validated.

References: [Windows themes](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-windows-themes), [composition tailoring](https://learn.microsoft.com/en-us/windows/apps/develop/composition/composition-tailoring), [high contrast](https://learn.microsoft.com/en-us/windows/win32/winauto/high-contrast-parameter).
