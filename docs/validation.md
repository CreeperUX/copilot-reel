# Validation status

Snapshot: 2026-10-19, Windows 11 25H2 build `26200.9711`, x64. Installed application: **0.3.0.0**. This records demonstrated behavior and its limits, not full release acceptance.

## Verified in v0.3.0

| Area | Evidence and scope |
| --- | --- |
| Native build | C++20 Win32 / Windows Composition / Direct2D / DirectWrite build passed with MSVC `/W4`. |
| Core geometry and state | 1-12 sectors, 96-384 DPI geometry, negative monitor origins, eight boundary positions, center-entry gating, delayed Down/Up ordering, exactly-once release, radial action routing, preset pagination, and protocol parsing passed. |
| Configuration | 48 parser/validation checks passed. Invalid data is preserved; fallback does not overwrite it. |
| Preferred profile | 65 checks passed, including rename/reorder stability, deleted preference fallback, data-directory isolation, UTF-16 BOM, corrupt preferences, and atomic replacement failure. |
| Native end-to-end input | `tests/run-radial.py` passed 13 scenario groups against the final Release binary. Cold/warm Tap, cold/warm Down, stop, duplicate Down/Up, orphan cold Up, protocol Tap, and the registered message contract are covered using harmless launch probes. |
| Animation interruption | Two additional regression groups passed: release 28.49 ms into opening canceled without launching; reopening 18.5 ms into exit remained visible after 270 ms. The old close timer did not hide the new gesture. |
| Missing preset | Selecting unresolved Claude Science opened the focused editor with its official name and an empty target. Cancel preserved the configuration byte-for-byte and launched nothing. |
| Actual pointer placement | On this 200% DPI display, the canvas measured 768 pixels. Normal tether measured 336.58 pixels against a nominal 336; all eight edge/corner positions stayed in the 2880 x 1800 work area. Tests verified the captured pointer and that the application did not warp it. |
| Radial navigation | Main, Settings, Preferred, Manage, entry actions, presets, next/previous, custom editor, and cancellation back to Manage passed. Center and Escape returned one level. |
| Native language input | Actual `SendInput` A/D and both language arrows switched and persisted English/Chinese. Testing found Chinese IME interception; only the command HWND now disables IME. Text editors retain IME input. |
| Rendering work at rest | Stationary visible surface draws stayed 52 to 52 over 350 ms; hidden draws stayed 119 to 119 over 350 ms. This verifies absence of repeated surface rasterization in those observations, not GPU cost or high-refresh presentation quality. |
| Official artwork | All 36 artwork hashes match provenance. Twenty light/dark draws passed and were visually reviewed through WIC/Direct2D and SVG-capable D3D11/WARP. Missing desktop installations remain empty targets for the focused editor. |
| Actual wheel images | Windows Graphics Capture captured the application HWND at 192 DPI, independently of overlapping desktop windows. Main and Settings images show official marks, matching geometry, readable controls, and no renderer error. See `output/runtime/v0.3.0/`. This validates the tested dark, opaque setup. |
| VI | v0.6 PDF contains 12 English pages; all pages were rendered and visually checked. The active vector masters describe radial hierarchy, original official marks, pointer tether, and native motion. |
| Clean installation | Old 0.2.0.0 package was removed, signed 0.3.0.0 installed, and two existing configuration files restored byte-for-byte. Installed executable SHA-256 equals the final tested build. All 39 packaged artwork/provenance/license files match source hashes. |
| Copilot registration | Installed provider and all three state declarations are valid. AUMID remains `CopilotReel.Desktop_nks6vs6sjb1qw!App`. The user's current Windows selection is this app. No tool wrote the key preference. |

Final controller evidence: `artifacts/qa/radial-20261019-221818-4f4830/results.json`.
Additional interruption/preset evidence: `artifacts/qa/radial-20261019-232122-f813fc/results.json`.
Installation receipt: `artifacts/installation.json`.
Configuration backup: `artifacts/backups/20261019-221732`.
These local evidence and signing directories are ignored by Git. Private signing material is never packaged.

The prior Windows Settings UI Automation check observed a visible, enabled Copilot Reel item. The new package retains that provider identity. Manifest verification and the saved current selection are not a new UI Automation observation of an open picker.

## Remaining checks and known limits

- Real physical Copilot short-tap/hold/release delivery, including hidden-window discovery and pathological Shell scheduling. Automated commands and messages do not establish hardware timing.
- Presentation/resource measurements at 60/144/240 Hz, mixed refresh rates, battery/energy saver, and a longer idle baseline. Compositor animation alone does not guarantee smoothness on every GPU.
- Live frosted-backdrop appearance, high contrast, reduced motion, device loss, mixed-DPI multi-monitor transitions, and small-work-area usability.
- The complete wheel remains inside the work area. At taskbar/out-of-work-area anchors, the off-canvas portion of the decorative tether can be clipped.
- Cold, running, and minimized instances of every preset application. Discovery is verified on this machine for ChatGPT/Codex, Claude, ZCode, and VS Code; the remaining presets ask for their actual installed target.
- Comprehensive accessibility/UI Automation semantics for custom radial controls. The focused text editor uses native controls.

## Repeatable checks

```powershell
.\tools\build.ps1
python .\tests\run-radial.py
.\tools\verify-registration.ps1
```

The radial integration fixture requires no resident Copilot Reel instance, creates a fresh isolated directory, launches only the harmless probe, and restores the pointer. Avoid desktop interaction while it runs. Historical v0.2 tests are archived in `tests/legacy-v0.2/`; they do not describe current UI acceptance.

`tools/install-development.ps1 -PackagePath <signed-msix>` validates signature, identity, and tested executable before removing the exact old package. It shuts down safely, backs up configuration, performs uninstall/install, restores verified data, and checks installed binaries/assets. It does not import certificates or change the Copilot target.

`tools/build-capture-tool.ps1` builds a standalone Windows Graphics Capture helper. `tools/capture-native-wheels.py` captures only the explicitly selected application window using isolated demo entries, even when another desktop window overlaps it. These are actual native renders, not PDF design scenes. Desktop screen captures were rejected when occlusion produced blank images.
