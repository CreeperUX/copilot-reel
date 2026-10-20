# Validation status

Snapshot: 2026-10-20, Windows 11 25H2 build `26200.9711`, x64. Installed application: **0.3.4.1**. This records demonstrated behavior and its limits, not full release acceptance.

## Verified in v0.3.4.1

0.3.4.1 passes on the foreground right before each launch and shows Claude Science with its own mark. Selecting the already running DeepSeek Harness in 0.3.4.0 launched without error, but its window stayed behind ChatGPT.

| Area | Evidence and scope |
| --- | --- |
| Native build | MSVC `/W4` build and all unit tests passed; iconography now has 134 checks (an installed Claude Science executable or shortcut keeps its own Shell icon). |
| Packaging | After the ARM64 change, `tools/package.ps1` refused every x64 package because it treated the unchanged x64 manifest as missing its architecture token; it now checks for the token itself. The signed `CopilotReel_0.3.4.1_x64.msix` passed `signtool verify /pa`. |
| Clean installation | 0.3.4.1 replaced 0.3.4.0 after a backup (`artifacts/backups/20261020-211819`); three configuration files were restored. The installed executable SHA-256 `5EF65109…1F6C` equals the build, all 42 packaged asset files match, the provider declaration is valid, and the user's Copilot key selection is unchanged. |
| Artwork | A 192 DPI capture of the installed reel shows Claude Science's DNA mark beside the Claude mark. |
| Package context | A process started through ShellExecute from inside the Copilot Reel package has no package identity (`GetCurrentPackageFullName` returned `APPMODEL_ERROR_NO_PACKAGE`), so launched agents keep their own AppData. |
| ARM64 package | Cross-compiled with `tools/build.ps1 -Architecture arm64` (machine `0xAA64`, not run). `CopilotReel_0.3.4.1_arm64.msix` is signed with the same development certificate and passed `signtool verify /pa`; its executable SHA-256 is `776A7AAD…E219`. |

Not yet verified: whether a running target now comes forward after a physical Copilot-key hold. `run.log` records `Foreground grant unavailable` with the error code when the reel holds no foreground right at commit. `tests/run-radial.py`, language invariance and the capture suite were not rerun for this patch because they drive the pointer.

## Verified in v0.3.4

| Area | Evidence and scope |
| --- | --- |
| Native build and tokens | MSVC `/W4` build of the application, all unit tests and the launch probe passed. `tools/gen-vi-tokens.ps1 -Check` confirms `src/vi_tokens.h` matches `design/vi/tokens.json`. |
| Unit tests | Core (1-12 sectors, placement, ordering, routes; the hub and Manage keep a disabled Add slot at 8 apps), settings 48, behavior 65, iconography 132 (target-based identity including profile folders and look-alike hosts, artwork classes, plated tile clipping, centered glyph and Shell icons at 96 and 192 DPI), local launch 16, display names 51. |
| Native end-to-end input | `tests/run-radial.py` passed all 21 groups twice, including both native-picker groups: release 25-48 ms into the 167 ms opening cancels without launching; a cancel by `WM_CLOSE` and re-hold 25.7 ms into the 83 ms exit stays visible after 270 ms; launches remain exactly once. Evidence: `artifacts/qa/radial-20261020-185859-e515d3/`. |
| Placement | Interior anchors have 0.00 px center displacement; all eight edges and corners clamp minimally and arm only after center entry; no cursor warp. |
| Rendering at rest | Visible stationary surface draws stayed 51 to 51 and hidden draws 98 to 98 over 350 ms. Selection, hover and transitions never re-rasterize item artwork. |
| Shared language profile | `tests/run-language-invariance.py` passed twice: Main EN vs ZH and EN vs returned EN differ by 0 pixels; the hub differs by 0 pixels outside the EN/ZH tile. Chinese names render in Microsoft YaHei UI (no per-character JhengHei UI / Yu Gothic UI mix). Evidence: `artifacts/qa/language-invariance-20261020-185651-56cdbd/`. |
| Native visual states | `tools/capture-v034.py` captured Main idle, a vendor tile selected, the hub idle, a command selected, Language selected and an edge-gated hold for dark/light solid, dark/light frosted, the high-contrast code path, and 96/144/192 DPI into `output/runtime/v0.3.4/` (per-variant contact sheets `sheet-*.png`). The acrylic effect graph is created without error in both frosted variants. |
| Motion | Frame strips `motion-open-select.png`, `motion-drill.png` and `motion-commit.png` show the 83 ms fade with the 0.96 settle, the seat appearing and travelling along the orbit, the name crossfade with the ring giving way, drill-in with the tile handing off to the back chevron, drill-out, and the commit hold with exactly one probe launch. These are visual evidence, not frame-rate measurements. |
| Clean installation | The signed 0.3.4.0 package replaced 0.3.3.0 after a backup (`artifacts/backups/20261020-193438`); three configuration files were restored. The installed executable SHA-256 `B20F5456…5D47` equals the final build, all 42 packaged artwork/provenance/license files match source hashes, the provider declaration is valid, and the user's Copilot key selection is unchanged. |
| Post-review build | An independent code review led to fixes for capability events off the UI thread, a seat expression surviving a mid-travel hide, double-composited selected artwork, permanently cached failed Shell icon lookups, updates during the exit re-showing items, device-loss retry, and late launch-failure dialogs. On this final build (the installed executable) all unit tests pass, `tests/run-radial.py` passed all 21 groups again (`artifacts/qa/radial-20261020-193700-3d0b82/`, stationary draws 51 to 51, hidden 97 to 97), language invariance again showed 0 differing pixels (`artifacts/qa/language-invariance-20261020-193729-021589/`), and the captures in `output/runtime/v0.3.4/` were regenerated from it (`artifacts/qa/visual-v0.3.4-20261020-193740-82c3e9/`). |
| Official artwork | Plated marks (Claude, Perplexity, ZCode, DeepSeek Harness, Air) fill and are clipped to the 40 DIP tile; glyph marks share the per-brand optical scale; Windows Settings, Notepad and custom executables show their Shell icons; command glyphs come from Segoe Fluent Icons. Native contact sheets were reviewed at 96 and 192 DPI in light and dark. |

The captures use `COPILOT_REEL_VISUAL_OVERRIDE` and an isolated data directory. Window captures contain only the application HWND: the frosted captures do not show the DWM host backdrop or the desktop beneath, and `contrast=on` exercises the high-contrast code path with the current (non-high-contrast) system colors.

## Testing infrastructure review on 2026-10-20

The [appearance and performance testing plan](testing-plan.md) records the current coverage gaps, proposed injection seams, acceptance matrix, ETW/WPR reuse and implementation order. It is a plan, not additional application acceptance evidence.

- The local `trace-code-bridge` 0.2.0 suite passed all 79 tests. Its existing .NET ETL exporter CLI loaded successfully. Synthetic `correlate` and `review` runs completed; their input explicitly identifies itself as artificial.
- Readiness evidence is retained in `artifacts/qa/trace-bridge-reuse-20261020-200016/`. No new WPR session, target instrumentation, GPU/frame measurement or system-setting change was performed.
- The reusable WPR workflow provides bounded recording, unique instances, phase markers and ETL checks. Its existing graphics profile and WPA views require Copilot-specific names, paths and process filters; no Copilot recorder wrapper has been implemented yet.
- The bridge exports CPU samples and stacks, not GPU or DWM frame metrics. Its tests and synthetic correlation do not establish Copilot rendering correctness or performance.
- Existing visual overrides are partial environment isolation: high-contrast colors and accents remain host-dependent, and a forced material can override the earlier energy-saver result. Deterministic policy/rendering tests and real-system integration remain distinct planned work.

The proposed unified Copilot confirmation model is also pending. Current 0.3.4 still changes to click/preview navigation after entering a submenu. The testing plan lists the required cross-input and page-transition regressions without marking them passed.

## ARM64 package, cross-compiled 2026-10-20

The x64 and ARM64 releases share one source tree. There is no architecture-specific input or rendering branch. This host is x64, so the ARM64 binaries were compiled with `tools/build.ps1 -Architecture arm64` and were not executed. The existing pointer-injection suite was not repeated; later x64 acceptance belongs to the [testing plan](testing-plan.md). The installed x64 0.3.4 app was left in place.

| Check | Result |
| --- | --- |
| Compiler | MSVC 14.44.35229 `Hostx64\arm64\cl.exe`, `/std:c++20 /W4 /O2 /MT`, Windows SDK 10.0.26100.0 ARM64 import libraries |
| Application | `build/arm64/Release/CopilotReel.exe`, `dumpbin` reports `AA64 machine (ARM64)`, PE32+, Windows GUI subsystem. SHA-256 `E9237A3132A361C61C94B7590B9A0051036BDEA5D0D38D775E2896A2A055DFA2` |
| Unit-test binaries | `core_tests`, `settings_tests`, `behavior_tests`, `iconography_tests`, `local_launch_tests`, `display_names_tests`, and `launch_probe` are all machine `0xAA64`. They were compiled and not started |
| Host execution | `CreateProcess` on `CopilotReel.exe` failed with "The specified executable is not a valid application for this OS platform." |
| Package | `artifacts/CopilotReel_0.3.4.0_arm64.msix`, signed with the development certificate `CN=CopilotReel Development` (`061ED5990CEBC4F2D7C252ABE3EFE7EB99D6239F`). `signtool verify /pa` succeeded. The staged manifest says `ProcessorArchitecture="arm64"`. `packaging/AppxManifest.xml` remains the x64 source manifest, and `artifacts/package` was not replaced |
| Installation | Not installed. `tools/install-development.ps1` rejects any package whose architecture is not x64 |

The signature matches the x64 development package. Installation and Copilot-key registration remain an ARM64-machine step. Acrylic, high contrast, physical Copilot-key timing, and the unit-test assertions are unchecked on ARM64.

## Verified in v0.3.3

| Area | Evidence and scope |
| --- | --- |
| Native build | C++20 Win32 / Windows Composition / Direct2D / DirectWrite build passed with MSVC `/W4`. |
| Core geometry and state | 1-12 sectors, 96-384 DPI geometry, negative monitor origins, eight boundary positions, center-entry gating, delayed Down/Up ordering, exactly-once release, radial action routing, preset pagination, and protocol parsing passed. |
| Configuration | 48 parser/validation checks passed. Invalid data is preserved; fallback does not overwrite it. |
| Preferred profile | 65 checks passed, including rename/reorder stability, deleted preference fallback, data-directory isolation, UTF-16 BOM, corrupt preferences, and atomic replacement failure. |
| Native end-to-end input | `tests/run-radial.py` passed 19 interaction groups; the two native-picker groups passed in a focused rerun after correcting a fixture race with dialog initialization. The application picker code was unchanged. Cold/warm Tap, cold/warm Down, stop, duplicate Down/Up, orphan cold Up, protocol Tap, and the registered message contract are covered using harmless launch probes. |
| Animation interruption | Release 25.79 ms into the 160 ms opening canceled without launching; reopening 18.3 ms into the 120 ms exit remained visible after 270 ms. The old close timer did not hide the new gesture. |
| Missing preset | Selecting unresolved Claude Science opened the focused editor with its official name and an empty target. Cancel preserved the configuration byte-for-byte and launched nothing. |
| Actual pointer placement | On this 200% DPI display, the canvas measured 768 pixels. Interior anchors (1440, 2700) and (912, 2958) had zero center displacement. All eight edge/corner positions used only the minimum translation and stayed inside the tested monitor work area [0, 1800, 2880, 3600]. Tests verified the captured pointer and that the application did not warp it. |
| Radial navigation | Main has one + entry, opening a combined hub for Language, Appearance, Preferred, Manage, and Add. Both preset invocation paths return to their recorded parent. Entry actions, pagination, custom editor, and cancellation back to Manage passed. Center and Escape returned one level. |
| Functional hover labels | Twenty-six observations verified real mouse and keyboard selection across English/Chinese hub actions, both language arrows, main app/+ items, and idle clearing. One name is rendered; no explanatory second line is added. |
| Local short tap | Native picker cancellation preserves data and launches nothing. Accepting the harmless executable saves the profile, retains a backup, launches once, and makes the next Tap direct. Sixteen shortcut-verification checks reject URL/missing targets without execution. Exact legacy ChatGPT/Claude homepage migrations preserve display names and preferred identity. Installed-user profiles now point to their native AUMIDs; the preferred index remains ChatGPT. |
| Built-in display names | 51 checks passed across English, Chinese, and English again. Historical Notepad/Windows Settings names translate only for display, including in the focused editor. Names, targets, custom labels, and launch identity remain intact. |
| Shared language profile | One native instance used one isolated apps.ini and behavior.ini while switching EN to ZH to EN. Both files retained identical SHA-256 hashes, app order and preference remained fixed, and all captures used the same dataPath. |
| Pixel-level icon invariance | Main EN versus ZH: zero differing pixels. Main EN versus returned EN: zero. Hub outside the EN/ZH tile: zero. Hub EN versus returned EN: zero. Actual captures are in output/runtime/v0.3.3/. System hover names show the appropriate language; only the locale monogram changes in the idle hub. |
| Native language input | Actual `SendInput` A/D, the EN/ZH tile, and both arrows beside that tile switched and persisted English/Chinese. Testing found Chinese IME interception; only the command HWND now disables IME. Text editors retain IME input. |
| Rendering work at rest | Stationary visible surface draws stayed 88 to 88 over 350 ms; hidden draws stayed 216 to 216 over 350 ms. This verifies absence of repeated surface rasterization in those observations, not GPU cost or high-refresh presentation quality. |
| Official artwork | All 39 artwork hashes match provenance. Thirty-five icon checks passed: strict target-based Windows Settings/Notepad glyphs, nonempty/distinct WIC renders, and SVG-capable D3D11/WARP bounds checks. Gemini and Perplexity use official artwork. Perplexity physical-unit SVG dimensions are normalized in the cached document so no pixels escape its optical bounds; original source files remain unchanged. |
| Actual wheel images | Windows Graphics Capture captured the application HWND at 192 DPI, independently of overlapping desktop windows. Main and combined-hub images show the six default identities, EN/ZH, functional hover names, matching geometry, and no renderer error. See `output/runtime/v0.3.2/`. The unchanged motion implementation was captured in v0.3.1; that sequence is visual evidence, not a frame-rate benchmark. This validates the tested dark, opaque setup. |
| VI | v0.8 PDF contains 12 English pages, all rendered and visually inspected. The active vector masters describe the single + entry, one-line functional hover names, EN/ZH and sun/moon state, zero interior pointer offset, and restrained native motion. Evidence: `artifacts/qa/vi-v08/results.json`. |
| Clean installation | Old 0.3.2.0 package was removed, signed 0.3.3.0 installed, and four existing configuration files across the installed/development scopes were restored byte-for-byte. This release does not rewrite saved names when language changes. Installed executable SHA-256 equals the final tested build. All 42 packaged artwork/provenance/license files match source hashes. |
| Copilot registration | Installed provider and all three state declarations are valid. AUMID remains `CopilotReel.Desktop_nks6vs6sjb1qw!App`. The user's current Windows selection is this app. No tool wrote the key preference. |

Controller evidence: `artifacts/qa/radial-20261020-132622-600967/results.json` (19 groups, then a picker-fixture timeout).
Focused picker rerun: `artifacts/qa/radial-20261020-132834-3f2e1d/results.json` (both picker groups passed).
Language/image invariance: `artifacts/qa/language-invariance-20261020-132456-0e03b9/results.json`.
Native motion evidence: `artifacts/qa/quiet-motion/capture-results.json`.
Installation receipt: `artifacts/installation.json`.
Configuration backup: `artifacts/backups/20261020-132908`.
These local evidence and signing directories are ignored by Git. Private signing material is never packaged.

The prior Windows Settings UI Automation check observed a visible, enabled Copilot Reel item. The new package retains that provider identity. Manifest verification and the saved current selection are not a new UI Automation observation of an open picker.

## Remaining checks and known limits

- Real physical Copilot short-tap/hold/release delivery, including hidden-window discovery and pathological Shell scheduling. Automated commands and messages do not establish hardware timing.
- Presentation/resource measurements at 60/144/240 Hz, mixed refresh rates, battery/energy saver, and a longer idle baseline. Compositor animation alone does not guarantee smoothness on every GPU.
- Live frosted-backdrop appearance over real content (this machine has Windows transparency effects off), real high-contrast themes, reduced motion, device loss, mixed-DPI multi-monitor transitions, and small-work-area usability.
- Known limits in 0.3.4: during the 83-133 ms exit a click over the plate is ignored but not passed to the application beneath (`HTTRANSPARENT` forwards only within the thread); retargeting the seat twice within one spring settle measures the shortest arc from the previous target, so a 180-degree flick can swing the long way round.
- Cold, running, and minimized instances of every preset application. Discovery is verified on this machine for ChatGPT/Codex, Claude, ZCode, and VS Code; the remaining presets ask for their actual installed target.
- Comprehensive accessibility/UI Automation semantics for custom radial controls. The focused text editor uses native controls.
- ARM64 runtime. The 2026-10-20 package is a native ARM64 image checked by its PE header and by this x64 host refusing to start it. Behavior, appearance, and Copilot-key registration on ARM64 hardware are still open.

## Repeatable checks

```powershell
.\tools\build.ps1
python .\tests\run-radial.py
uv run --python 3.14 --with pillow python .\tests\run-language-invariance.py
uv run --python 3.14 --with pillow python .\tools\capture-v034.py
.\tools\verify-registration.ps1
```

The radial integration fixture requires no resident Copilot Reel instance, creates a fresh isolated directory, launches only the harmless probe, and restores the pointer. Avoid desktop interaction while it runs. Historical v0.2 tests are archived in `tests/legacy-v0.2/`; they do not describe current UI acceptance.

`tools/install-development.ps1 -PackagePath <signed-msix>` validates signature, identity, and tested executable before removing the exact old package. It shuts down safely, backs up configuration, performs uninstall/install, restores verified data, and checks installed binaries/assets. It does not import certificates or change the Copilot target.

`tools/build-capture-tool.ps1` builds a standalone Windows Graphics Capture helper. `tools/capture-native-wheels.py` captures only the explicitly selected application window using isolated demo entries, even when another desktop window overlaps it. These are actual native renders, not PDF design scenes. Desktop screen captures were rejected when occlusion produced blank images.
