# Validation status

Snapshot: 2026-10-19, Windows 11 25H2 build `26200.9711`, x64. This is a development validation record, not a release-readiness declaration.

## Verified

| Area | Result and scope |
| --- | --- |
| Toolchain | Seven pinned Microsoft archives passed SHA-256 checks and extraction. Compiler, resource compiler, and MSIX packager have valid Microsoft Authenticode signatures. |
| Build | The native C++20 Win32 application compiled and linked with the local MSVC/SDK toolchain. |
| Core behavior | Tests passed for gesture selection, cancellation/deduplication, hit testing, private URI parsing, full-footprint placement, negative monitor coordinates, and center-entry gating. |
| Configuration | 48 checks passed for parsing, escaping, validation, and boundaries. |
| Localization | Native OSD controls were exercised in an isolated directory: English on first launch, Language only in second-level Settings, switch to Simplified Chinese, persistence across restart, and switch back to English all passed. Preferences now use an atomic UTF-16 write and rename. |
| Actual controller integration | An isolated application instance accepted another process's command, selected through Raw Input, and launched the harmless probe once on release. Repeated release did not duplicate activation. |
| Cancellation/messages | Center release canceled. Synthetic fast-path messages exercised the declared contract and canceled release. Process shutdown was clean. |
| Runtime renderer | The recorded diagnostic used hardware rendering at DPI 192, dark appearance, transparency disabled, animations enabled, and six targets. A runtime screenshot was captured. This covers that solid-mode setup, not all material modes. |
| Default placement | Interior launch remained pointer-centered. Near boundaries, the complete footprint is minimally translated into the monitor work area with the appropriate center-entry guard. |
| Edges and corners | Native tests passed at four edges and four corners on a 2880 x 1800 work area at 200% DPI. Each full 768 x 768 window remained inside bounds. Opening did not move the pointer; early and pre-center releases canceled; center entry followed by selection launched exactly once, producing eight harmless probe launches. |
| Native OSD | Home/Apps/Appearance/Settings navigation, separate Advanced editing, rename/add persistence, and native language controls passed. Normal-size scrollbars are absent. Dark combo boxes and label backgrounds were visually reviewed after their final styling fixes. |
| Unified symbols | The wheel and Apps list use the same native vector symbol function. Mixed shell bitmaps and letter initials have been removed. Actual rendered icons and native OSD screenshots were reviewed. |
| Development package | Version 0.2.0.0 was signed and verified, then cleanly reinstalled after backing up configuration. Two existing configuration files were restored byte-for-byte. The installed executable hash matches the tested build. The previously authorized development certificate ending `99D6239F` remains in `LocalMachine\TrustedPeople`. |
| Windows Settings recognition | UI Automation previously observed a visible, enabled `Copilot Reel` item in the actual keyboard customization control. The reinstalled 0.2.0 package has the same AUMID and valid provider/state declarations. The current key choice is user-controlled; no tool writes it. |

Local evidence is in ignored `artifacts/qa` and `artifacts/signing` directories. The integration results explicitly distinguish hardware/Settings checks from synthetic inputs. Private signing material is not included in the repository or application package.

## Pending

- Physical short-tap, hold-start, and release delivery, including hidden/non-activating receiver discovery.
- URI cold activation and release during startup, with ordering and cancellation.
- Real frosted-background appearance, circular clipping, and solid fallback; high contrast, disabled animations, and live changes.
- Multiple monitors, mixed DPI, unusually small work areas, and mixed refresh rates. The normal-monitor boundary cases above are covered; the broader matrix remains open.
- Measured presentation/resource use at 60/144/240 Hz, static/hidden states, and battery/energy saver.
- Existing, minimized, and cold instances of each supported target application, including foreground activation.

Settings recognition is established for the tested signed development package on build 26200.9711. That result does not establish physical-key delivery or eligibility of every signer and Windows version.

## Repeatable checks

```powershell
.\tools\bootstrap-toolchain.ps1 -VerifyOnly
.\tools\build.ps1
.\tools\verify-registration.ps1
```

`tests/run-integration.py` requires Python and the built application/test probe. It uses isolated data and restores the pointer. Exit any resident Copilot Reel instance before running it. It does not select a provider in Settings or press a physical key.

`tests/run-edges.py` exercises the complete boundary/guard/launch flow and OSD editing. `tests/run-localization.py` verifies the secondary language page and restart persistence. `tests/run-placement.py` verifies ordinary or minimally clamped placement. All use isolated data; pointer-moving tests restore the original position.

`tools/inspect-copilot-providers.ps1` inventories packages read-only. `tools/inspect-settings-ui.ps1` captures a read-only UI Automation snapshot with Windows PowerShell 5.1 while Settings is open. Neither changes the selected key binding.
