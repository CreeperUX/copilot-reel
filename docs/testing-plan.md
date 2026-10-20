# Appearance and performance testing plan

This plan extends the 0.3.4 application and VI v0.9 baseline. It combines deterministic appearance tests with native rendering and ETW/WPR measurements, and defines regression coverage for the proposed unified menu interaction. Existing results belong in [validation.md](validation.md); the work below is planned unless explicitly marked verified.

The product has two packages, x64 and ARM64, from one source tree. ARM64 is cross-compiled on the x64 development host and is not part of this host's runtime matrix. Further x64 appearance, interaction, and performance acceptance uses the layers in this plan. The existing pointer-injection suite remains historical evidence for 0.3.4; it is not the acceptance run for later x64 work.

Reviewed on 2026-10-20 against application commit `47a63e9` and VI commit `2bc3534`. No new Copilot Reel ETW recording, system-setting change, or interaction implementation was performed during this planning review.

## Existing coverage and gaps

| Existing component | What is covered | Remaining gap |
| --- | --- | --- |
| `tools/capture-v034.py` | Seven variants covering light/dark solid and frosted requests, the high-contrast branch, and 96/144/192 DPI; native stills and motion strips | Mostly checks labels and absence of renderer errors. It does not assert every expected material/fallback or measure frame pacing. |
| `COPILOT_REEL_VISUAL_OVERRIDE` | Process-local theme, material, contrast, motion and DPI overrides; no persisted system changes | Accent colors, high-contrast palette, energy saver, device capabilities and creation failures are not injectable. The override is parsed once per process. |
| `AppearanceSource::Read` | Reads live Windows preferences and resolves visual roles | Collection and policy are combined. `contrast=on` still uses the current `GetSysColor` values. A forced frosted override is applied after the energy-saver decision and can re-enable transparency. |
| `OrbitRenderer::ApplyMaterial` | Real capability checks, DWM host-backdrop setup and solid fallback | Results still depend on the host GPU, DWM and actual creation outcomes. A successful request is not proof of visible background blur. |
| `CreateAcrylicBrush` | Production luminosity, tint and deterministic noise graph | It creates its own host-backdrop brush. The shell supplies the blurred background; the graph does not perform an additional blur. |
| Interaction and identity tests | Launch deduplication, placement, nested menus, local targets, language persistence and pixel invariance | Menu-open Copilot confirmation and comprehensive appearance transitions are not covered by the current behavior contract. |

The existing frosted HWND captures exclude the desktop host backdrop. The existing forced-contrast captures use the ordinary system palette. Preserve these as branch/capture evidence, not complete acrylic or real high-contrast acceptance.

## Three testing layers

### 1. Deterministic policy

Separate Windows collection from policy without creating another UI process. The following interfaces are proposed, not current APIs:

```cpp
SystemVisualState ReadSystemVisualState();
Appearance ResolveAppearance(const SystemVisualState& state);
MaterialPlan ResolveMaterial(const SystemVisualState& state,
                             const MaterialCapabilities& capabilities);
```

`SystemVisualState` should contain light/dark mode, transparency requested, animations enabled, high contrast, energy saver, the accent ramp and the complete contrast palette. `MaterialCapabilities` should distinguish supported effects, fast effects and software rendering. The renderer executes the plan and reports an actual creation failure separately.

Apply test overrides to the input snapshot before policy evaluation. A test that requests acrylic while also injecting high contrast or energy saver must still obtain solid. To test acrylic independently of the host's power state, explicitly inject `energySaver=false`; do not bypass the production precedence rules.

Record requested material, resolved material, effective material, fallback reason and palette source. Distinguish `HighContrast`, `EnergySaver`, `TransparencyDisabled`, `EffectsUnsupported`, `EffectsSlow`, `SoftwareDevice` and `CreationFailed`. Missing capabilities must have a documented conservative policy, not an assumed successful effect.

### 2. Deterministic native rendering

Make the acrylic graph accept a backdrop brush supplied by its caller. Production supplies the real host backdrop. Tests supply known color fields or a fixed preblurred image and use the same luminosity/tint/noise graph. This tests material composition, alpha and clipping; it does not test how DWM generates the blur.

Use fixed accent and high-contrast palettes, fixed artwork, the shared VI tokens and explicit DPI. Keep reference images and color/bounds assertions separate: font and GPU differences may require documented pixel tolerances, while geometry, state and fallback invariants remain exact. Do not update reference images automatically to make a failing test pass.

| Matrix area | Required cases | Acceptance |
| --- | --- | --- |
| Base and material | Light/dark solid; light/dark injected acrylic | Expected color roles, source kind, alpha and circular clipping; no accidental desktop dependency |
| High contrast | Black background/white foreground and inverse palette, each with explicit highlight colors | System-role colors replace decorative roles, no backdrop/shadow, readable selected and disabled states |
| Precedence | Transparency off, energy saver, high contrast, unsupported/slow effects, WARP, factory failure | Exact effective material and fallback reason; no silent successful-acrylic claim |
| Item states | Idle, hover, selected, disabled, preferred badge, edge gate, Back and EN/ZH | Correct glyphs, labels, order, focus and hit geometry |
| Scale | 96/144/192 DPI plus geometry tests across the supported range | No clipped icons or labels; rendering and hit testing use the same final center |
| Reduced motion | Motion off and high contrast: open, selection, drill in/out, commit, cancel and reopen during exit | Correct final state; no surviving leaving page, delayed launch or orphaned seat expression. Existing implementation uses immediate paths or 1 ms animation durations. |
| Live changes | Solid/acrylic, normal/contrast, motion on/off, capability loss/recovery, device recreation; include changes during exit | Preserve menu/selection and recover once; no stale backdrop, invisible interactive items or reappearing exit content |
| Resource lifetime | Repeat identical appearance, stationary menu, hidden menu, rapid appearance changes | No unchanged-surface redraw loop; release obsolete resources and rebuild only when required |

Keep `COPILOT_REEL_DATA_DIR` isolated. Until override handling is refactored, start a fresh process per environment fixture because `CurrentVisualOverride()` caches its first value. A dynamic-change test should inject a new snapshot/event through a test seam, not mutate that cached environment variable.

### 3. Real Windows integration and performance

Retain a smaller real-system matrix for actual high-contrast themes, transparency and energy-saver notifications, DWM host backdrop, hardware capability changes and mixed displays. Use a controlled background window to assess the visible material over content. ETW measurements and visual captures answer different questions and should remain separate evidence.

Do not change the user's persistent settings to run deterministic tests. Real-system changes, when part of an authorized experiment, need the original values recorded and restored. Keep ASUS services, OLED/Pixel Shift, wallpaper ownership and unrelated display settings unchanged for Copilot tests.

## Reusing the ETW/WPR workspace

The existing local workspace is a separate checkout outside this repository. Its components are external development tools, not runtime dependencies of Copilot Reel. Paths below identify the reviewed installation; future wrappers must accept dependency locations rather than hard-code this machine.

| Component relative to that workspace | Reuse | Required adaptation |
| --- | --- | --- |
| `scripts/wallpaper/Rotation.wprp` | CPU sampled stacks, CSwitch/ReadyThread, process/module context, GPU, DWM, Win32k, DirectComposition and WPR markers | Derive a Copilot-specific profile and retain source provenance/hash. Configured system/event buffers total 256 MiB; File-mode output can still grow. |
| `Control-RotationTrace.ps1`, `Record-RotationTrace.ps1`, `Etw.Common.ps1` in that directory | Bounded worker, unique WPR instance, queued commands, status files, profile hash, automatic stop/save | Existing scripts are LPT-03-specific and use wallpaper paths, phases and names. Preserve them; adapt a separate wrapper with `CopilotReel-<run-id>`, explicit output paths and no wallpaper probe. |
| `Test-RotationTraceFile.ps1` and `RecorderControl.wpaProfile` | Actual WPAExporter decoding, marker-range resolution and loss checks | Parameterize the ETL and output path. Marker success in a worker log is insufficient until markers are decoded from the ETL. |
| `RotationGraphics.wpaProfile` | DWM frames, GPU execution packets and CPU scheduling tables | Its CPU view currently filters `dwm.exe`. A Copilot profile must retain DWM and add the application/process instance; do not overwrite the original profile. |
| `trace-code-bridge/adapters/dotnet` | TraceEvent 3.2.8 offline ETL-to-runtime export with process/time filters | Exports CPU sampled IPs and stacks only, not GPU packets, DWM frames, CSwitch, function parameters or generic Present events. |
| `trace-code-bridge` 0.2.0 | Offline address/lifetime/identity correlation and evidence-gap review | Current `index` consumes REA evidence. Prefer matching PDBs in WPA for our own source; any future PDB adapter must identify its source honestly. |

Verified reuse readiness on 2026-10-20: all 79 bridge unit tests passed; the existing ETL exporter CLI loaded; synthetic `correlate` and `review` completed. Evidence: `artifacts/qa/trace-bridge-reuse-20261020-200016/readiness.json` and adjacent synthetic outputs. These are tooling checks, not Copilot runtime or performance measurements. No new recorder was started.

The bridge reports version 0.2.0, but its current local checkout has no Git commit and contains untracked source. Before a measured run, retain a content-hashed source/tool snapshot; do not invent a revision pin. The reviewed `Rotation.wprp` SHA-256 is `DC8DE29936A9E28169095CDCF313A63B4FD87336001218B56D046799519CFF2F`.

## Capture and evidence contract

Use short, bounded File-mode recordings, initially 20-40 seconds per scenario. Keep an uninstrumented visual run and separate instrumented performance runs so screenshot capture does not silently change the measured compositor workload. Include an app-closed baseline and a resident-hidden baseline under the same display and power conditions.

Each run should retain:

- Run/scenario ID, application version, source revision or dirty-source snapshot, tokens hash, actual EXE/PDB hashes, compiler/linker flags and matched PDB identity.
- Actual process ID and creation time, HWND, monitor/DPI/refresh mode, power source, requested and effective appearance, injected-versus-system origin and capability/fallback details.
- ETL, profile hash, recorder/decoder versions, start/end markers, status/worker logs, dropped-event/buffer information, selected analysis interval and table exports.
- Application phase events, optional visual evidence, diagnostics, configuration isolation path and cleanup result. Store private traces and symbols under ignored `artifacts/perf/<run-id>`; do not package or upload them by default.

Use only the recorder instance owned by the run. Do not cancel another WPR session or discard an incomplete trace. Preserve timed stop, disk-space checks and failure evidence. Elevate only the bounded recorder when required; application/input fixtures stay at normal integrity. The current wallpaper controller's `-NoProbe` suppresses its unrelated probe, but its machine/path coupling still requires adaptation.

Before recording, verify profile/provider availability, recorder/exporter versions, available disk capacity and conflicting active sessions. Record unsupported capabilities or missing required events as untested/inconclusive, never as a performance pass.

The existing marker queue is serviced about every 100 ms. It is adequate for multi-second scenario boundaries, not input-to-frame latency or exact 83-250 ms animation timing. Add an application TraceLogging provider for precise phase events in the ETW clock domain. Proposed events include input received, selection changed, page transition requested, animation submitted/completed, material resolved/applied/fallback, device recovery and launch dispatch. Include session/gesture ID, page generation, selection and material reason; omit user paths and launch arguments unless required by a specifically scoped trace.

An animation-submitted or completion callback is not proof of presentation. Correlate it with the available composition/presentation evidence and record when the first-visible-frame boundary cannot be determined. Keep request, receipt, compositor completion and observed display timing distinct.

## Performance scenarios and measurements

| Scenario | Comparison and measurements |
| --- | --- |
| App closed / resident hidden | CPU/GPU baseline, wakeups, private memory and residual composition activity |
| Visible, stationary solid / acrylic | Incremental application and DWM cost; unchanged surface draws; material-specific activity |
| Repeated open/cancel and open/commit | First-visible latency where observable, tail latency, CPU ready delay, allocation/rebuild activity, exactly-once launch |
| Rapid selection, including reversal across 180 degrees | Seat retargeting, long-arc regressions, frame continuity and unnecessary artwork redraws |
| Drill in/out and preset paging | Page handoff, retained center, input continuity, old-page cleanup and queued-input correctness |
| Appearance and capability transitions | Fallback/recovery latency, resource reconstruction and absence of stale/invisible layers |
| 60/144/240 Hz where available, mixed DPI/refresh, AC/battery | Distribution of presentation intervals, missed opportunities and incremental cost under recorded hardware conditions |

Report sample count, run duration, median and tail distributions with comparable baselines; select acceptance budgets before comparing implementations. Nominal display periods are 16.67 ms at 60 Hz, 6.94 ms at 144 Hz and 4.17 ms at 240 Hz. They are reference intervals, not measured application frame times or automatic pass thresholds. Mark unavailable hardware modes as untested rather than simulating a physical refresh rate with a timer.

Windows Composition can continue animating with no application surface redraws. Inspect DWM/GPU as well as `CopilotReel.exe`. A DWM frame belongs to desktop composition and must not automatically be attributed to this wheel. Few or no application swap-chain Present events do not prove that a compositor animation failed to render.

For CPU attribution, add an optimized Profile/RelWithDebInfo build using `/O2`, debug information and matching PDB output, preserving release-equivalent optimization/link settings. Current Release does not emit a PDB. Do not benchmark an `/Od` Debug build as Release. Use PDB GUID+Age to match symbols; retain capture-time binary identity separately. A later on-disk hash cannot prove which bytes were loaded in an earlier trace.

Decode graphics/scheduling tables through WPAExporter and feed only CPU-location evidence to the bridge. Its `coverage.selected_samples` counts samples; sampled IP plus stack observations are not function calls. Exporter exit code 2 means reported loss/truncation, not an ordinary successful run. Do not use WPAExporter switches that suppress loss or time-ordering errors. No sample, no matching function, or an empty graphics table must remain a coverage limitation.

## Proposed unified interaction regression

The recommended interaction contract is still planned. Current 0.3.4 enters click/preview mode after `Navigate`, ignores hold-start while a gesture is active, and treats Tap as preferred-app activation. The proposed change routes all confirmation inputs through one action while preserving the idle short-tap shortcut.

| State | Proposed behavior |
| --- | --- |
| Menu closed | Tap launches the preferred local app; HoldStart opens Main |
| Any menu open | Tap, HoldStop from a new valid hold, click and Return confirm the current selection; HoldStart continues on the current page |
| Confirm an application / container / value | Launch once and close / enter child and retain the menu / adjust and retain the current page |
| Back | Center Back, Esc or Backspace returns one level; root cancellation closes |
| Native editor or file picker open | Copilot events cannot launch the preferred app or confirm behind the dialog |

Test input parity at every depth; no-selection/disabled confirmations; duplicate and stale releases; no extra preferred launch while a menu is open; page-generation invalidation; old pointer/queued-click positions after transitions; local adjustment keys only when their value item has focus; cancellation and focus restoration; appearance changes during navigation. One release must never both enter a container and execute a child item. Keep this state testing independent of animation completion.

ASUS's OSD provides a consistent select/confirm/back model across levels; macOS menus support different input methods, while Control Center distinguishes toggling from expanding details. These motivate the proposed contract, not a claim that either platform specifies this radial implementation. Dwell-to-open submenus remain an optional experiment, not an accepted default.

## Implementation sequence

| Order | Deliverable | Exit condition | Status |
| --- | --- | --- | --- |
| 1 | Pure appearance/material policy with complete injected snapshots | Deterministic precedence, palette and fallback tests; no Windows mutation | Planned |
| 2 | Injectable acrylic source and native visual fixtures | Solid, synthetic-acrylic, contrast and reduced-motion matrix with explicit source provenance | Planned |
| 3 | Shared Confirm/Back/Adjust routing | Input parity across menu levels, no event penetration, no shortcut behind an editor | Proposed interaction, not implemented |
| 4 | Optimized symbol-bearing build and TraceLogging phases | Matched artifacts, phase IDs and documented timing semantics | Planned |
| 5 | Copilot-specific WPR controller/profile/export views | Bounded owned session, decoded markers/loss checks, correct app and DWM filters | Planned; external tooling readiness verified |
| 6 | Controlled performance comparisons and real-system checks | Repeated baselines, separate visual evidence, recorded unknowns and hardware scope | Not run |

This documentation update is not an application release. When implementation produces a new release, retain the existing backup, uninstall, install, restore and installed-hash verification procedure.

## References

- [Current validation evidence](validation.md), [architecture](architecture.md), [configuration isolation](configuration.md), [toolchain](toolchain.md).
- [Microsoft Composition tailoring](https://learn.microsoft.com/en-us/windows/apps/develop/composition/composition-tailoring): user preferences, effect capabilities and cost.
- [Microsoft high-contrast state](https://learn.microsoft.com/en-us/uwp/api/windows.ui.viewmanagement.accessibilitysettings.highcontrast): system preference semantics.
- [ASUS VG27AQ3A-L guide, section 3.1.1](https://dlcdnets.asus.com/pub/ASUS/LCD%20Monitors/VG27AQ3A-L/ASUS_VG27AQ3A-L_EN.pdf?model=vg27aq3a-l): hierarchical navigation and confirmation.
- [Apple menu tracking](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/MenuList/Articles/ViewsInMenuItems.html) and [Control Center](https://support.apple.com/guide/mac-help/quickly-change-settings-with-control-center-mchlc9d0e1f2/mac): menu tracking and distinct adjustment/expansion actions.
