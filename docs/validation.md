# Validation status

Snapshot: 2026-10-19, Windows 11 25H2 build `26200.9711`, x64. This is a development validation record, not a release-readiness declaration.

## Verified

| Area | Result and scope |
| --- | --- |
| Toolchain | Seven pinned Microsoft archives passed SHA-256 checks and extraction. Compiler, resource compiler, and MSIX packager have valid Microsoft Authenticode signatures. |
| Build | The native C++20 Win32 application compiled and linked with the local MSVC/SDK toolchain. |
| Core behavior | Tests passed for gesture selection, cancellation/deduplication, hit testing, and private URI parsing. |
| Configuration | 48 checks passed for parsing, escaping, validation, and boundaries. |
| Localization | Native controls were exercised in an isolated directory: English on first launch, switch to Simplified Chinese, persistence across restart, and switch back to English all passed. |
| Actual controller integration | An isolated application instance accepted another process's command, selected through Raw Input, and launched the harmless probe once on release. Repeated release did not duplicate activation. |
| Cancellation/messages | Center release canceled. Synthetic fast-path messages exercised the declared contract and canceled release. Process shutdown was clean. |
| Runtime renderer | The recorded diagnostic used hardware rendering at DPI 192, dark appearance, transparency disabled, animations enabled, and six targets. A runtime screenshot was captured. This covers that solid-mode setup, not all material modes. |
| Default placement | Ordinary launch opened the reel at its captured pointer. Window center differed by no more than one physical pixel; this check did not move the pointer. |
| Development package | The signed development MSIX was installed. The user-authorized development certificate ending `99D6239F` was imported into `LocalMachine\TrustedPeople`. |
| Windows Settings recognition | UI Automation observed a visible, enabled `Copilot Reel` item in the actual Settings keyboard customization control. At that observation, the selected route matched `CopilotReel.Desktop_nks6vs6sjb1qw!App`. The 0.1.1 update remains registered with valid declarations; the latest route read was `ContextMenu`, with the application AUMID retained as the last app choice. No tool writes the selected route. |

Local evidence is in ignored `artifacts/qa` and `artifacts/signing` directories. The integration results explicitly distinguish hardware/Settings checks from synthetic inputs. Private signing material is not included in the repository or application package.

## Pending

- Physical short-tap, hold-start, and release delivery, including hidden/non-activating receiver discovery.
- URI cold activation and release during startup, with ordering and cancellation.
- Real frosted-background appearance, circular clipping, and solid fallback; high contrast, disabled animations, and live changes.
- Multiple monitors, mixed DPI, display boundaries, and mixed refresh rates.
- OSD navigation and guarded full-circle edge placement from VI v0.4 are specified but not implemented. The earlier half-orbit/fan designs are rejected.
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

`tools/inspect-copilot-providers.ps1` inventories packages read-only. `tools/inspect-settings-ui.ps1` captures a read-only UI Automation snapshot with Windows PowerShell 5.1 while Settings is open. Neither changes the selected key binding.
