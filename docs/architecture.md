# Architecture

## Platform and scope

Copilot Reel uses the official Windows Copilot hardware key provider contract and accepts its approximately 300 ms hold threshold. It does not install a keyboard driver or intercept Copilot with a global keyboard hook.

The implementation is one native C++20 process with a Win32 message loop. Windows Composition owns the visual tree and property animations; Direct2D and DirectWrite render geometry, icons, and text into reusable surfaces. The first visual scheme is Soft Orbit.

Application v0.2.0 and VI v0.5 implement an OSD quick menu with second-level Settings and focused app task sheets. Ordinary launch opens the pointer-centered reel. Placement preserves the full circle, minimally shifts it inside the monitor work area, and waits for center entry when the pointer begins outside its relocated cancellation zone. The earlier half-orbit/fan proposals are rejected.

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
| Visible + hold stop | Resolve once, hide, then activate |
| Idle + hold stop | Ignore without launching |
| Duplicate hold start | Preserve the current gesture |
| Short tap or preview | Show click-to-select preview |
| Right-click, session lock/disconnect, display/power interruption | Cancel |

Release is independent of animation completion. Gesture state owns deduplication and invalidates its generation when canceled. Cross-process cold-start ordering is not guaranteed by the contract; warm-instance tests do not establish that order. Cold-start races remain system-validation work.

## Window and pointer

The wheel is a non-activating tool popup. Opening preserves keyboard focus and records one cursor anchor. Raw Input is registered only while visible; pointer messages can also update hit testing. Movement within the same sector does not trigger selection repaint. Production code never warps the pointer.

Layout and hit testing share DIP geometry and the final visible center. The process is Per-Monitor DPI aware. Placement clamps the complete wheel footprint to the monitor work area without moving the pointer. The center-entry guard cancels an early release. Work areas too small to contain the wheel open the OSD instead; its native scrollbars preserve control sizes. Mixed-DPI transitions remain part of the display-validation matrix.

Closing unregisters Raw Input, stops the abandonment timer, hides the HWND, and removes visible composition work. The message loop remains. The timeout is not a frame or pointer-polling timer.

## Responsibility boundaries

UI and logic are separated within one process. The design currently needs neither a separate UI process nor a dedicated per-frame rendering thread.

| Component | Responsibility |
| --- | --- |
| Win32 controller | Copilot/URI commands, handoff, pointer input, tray, lifecycle |
| GestureState and hit testing | Selection, cancellation, release, deduplication, angular geometry |
| AppearanceSource | Windows colors, effects, animations, high contrast, power settings |
| OrbitRenderer | Direct2D/DirectWrite, composition visuals, backdrop, animation |
| Configuration/settings | OSD hierarchy, native semantic controls, focused editor, validation, persistence |
| Platform activation | Shell and packaged-app activation |
| Shared iconography | Deterministic monochrome Direct2D symbols for both wheel and OSD |

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

Device resources, text formats, and surfaces are reused. Content redraws on relevant selection, center-gate, appearance, target, DPI, or device changes. The wheel uses consistent 40 DIP tiles, 28 DIP vector symbols, and a 1.75 DIP stroke. Selected symbols follow the accent; high contrast uses system highlighted text. Unknown apps use a neutral application-window symbol. Shell bitmap extraction and initials fallback are removed. Opening uses a 100 ms compositor opacity/scale animation when enabled; otherwise it sets the final state. Selection and OSD page transitions are immediate.

There is no pre-rendered frame sequence or fixed 60 Hz application timer. The compositor evaluates animation at presentation cadence without application redraws on every refresh. This reduces work but does not guarantee frame rate or GPU cost.

Performance validation must cover 60/144/240 Hz, mixed refresh rates, different DPI, integrated graphics, and battery/energy saver. Measure first-visible latency, selection latency, frame-time variation, and static/hidden CPU/GPU activity against an application-closed baseline. See [validation](validation.md).

## Configuration and activation

The native editor supports 1-8 targets. The OSD home contains Apps, Appearance, and Settings. Language and About belong inside Settings. The editor exposes name and target; a separate compact Advanced sheet contains launch method, arguments, and working directory. The 560 x 360 DIP logical client area sits inside a native Windows caption and frame. Buttons, list boxes, edits, and the language combo retain native semantics. There is no autostart setting. App data stores names, types, identifiers, arguments, and working directories. See [configuration and isolation](configuration.md).

Shell targets use `ShellExecuteExW`; AUMIDs use `IApplicationActivationManager`. Existing instances should use each target's activation contract. Windows foreground rules apply; cold, running, and minimized states need per-application verification.

## Current milestone

Compilation, core/configuration tests, controller-to-test-launcher integration, and signed development installation succeeded. Actual Windows Settings recognition and the currently selected route were confirmed on build 26200.9711. Physical-key events, cold-start delivery, and backdrop/performance acceptance remain open. The prototype is not fully validated.

References: [Windows themes](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-windows-themes), [composition tailoring](https://learn.microsoft.com/en-us/windows/apps/develop/composition/composition-tailoring), [high contrast](https://learn.microsoft.com/en-us/windows/win32/winauto/high-contrast-parameter).
