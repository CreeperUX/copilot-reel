# Copilot key research

Research date: 2026-10-19. Observed system: Windows 11 25H2, build `26200.9711`.

## Initial observations

The original current-user inventory successfully read all 184 installed Appx/MSIX manifests. At that time, `OpenAI.Codex` was the only package declaring `com.microsoft.windows.copilotkeyprovider`, with provider display name `ChatGPT`. The installed Claude package had a launch protocol but no provider extension.

The selected key-route AUMID matched that provider. Settings showing only ChatGPT was consistent with this machine's registration state; it was not evidence that Windows permits only ChatGPT.

Microsoft's public contract requires package identity, signing, and the extension declaration. Installing an application or giving it a URI protocol is insufficient to make it a candidate.

## REA static analysis

REA 6.5.0 and Ghidra 12.1.4 inspected `SettingsHandlers_nt.dll`:

- File version: `10.0.26100.9711`.
- SHA-256: `bebc0caefd7f3cb017a511820722b260d6e1ec0c9bba0394d4ee11c60d826817`.
- Provider string address: `0x1803209c0`.
- Reference address: `0x180188a58`, in `FUN_180188a14`.
- The function obtains providers, checks candidate properties, constructs entries, and appends them to the list.
- No branch choosing candidates by the ChatGPT brand name was observed in that function.

| Observation | Evidence ID |
| --- | --- |
| Provider string search | `ev_1980a2504f2c0d6ed1c7b1a5d11a52a05a64ee08a80244a10023a7c5ff6dee9c` |
| Reference chain | `ev_a4e237547326ca2577dbfe63a5f1de43a5254a2c3f6457e35ccc7053d1bed017` |
| Candidate construction | `ev_43a96b610c9c073c2090c273cf23b3023ca082a314156e67c75164d6044c07aa` |
| Property helper | `ev_79c6c21dcc16fd91a4ad9890bacc420533fe8516803f10b47364b239d2541dc8` |

These IDs refer to the original research workspace. This repository preserves conclusions, not original sessions, machine inventories, registry exports, system binaries, or decompiler projects.

## Prototype follow-up

Copilot Reel now implements the documented extension and URI/fast-path states. A signed development MSIX is installed on the development machine.

A later UI Automation inspection of the actual Windows Settings keyboard page observed a visible, enabled `Copilot Reel` item in the Copilot customization control. The current selected route also matches `CopilotReel.Desktop_nks6vs6sjb1qw!App`, and registration verification reports `SelectedByUser=true`. This confirms recognition and the current selection on this machine. It does not establish that the inspection tool itself changed the selection or that every signing configuration will be accepted.

Automated tests exercised process handoff, Raw Input selection, release-to-launch, cancellation, and synthetic fast-path messages against the actual controller. These runtime results are distinct from the static analysis. Physical-key delivery remains unverified. See [validation status](validation.md).

## Evidence boundaries

- Internal COM filter property names remain unknown. Values `0..4` alone do not identify a property as the public `PackageSignatureKind` enum.
- Recognition of this development package on build 26200.9711 does not prove arbitrary development, self-signed, or enterprise packages will appear on other systems.
- The basic provider documentation's minimum build cannot be assumed to cover hold states or the fast path.
- Synthetic messages verify application handlers, not physical-key delivery, hidden-receiver discovery by Shell, or cold-start order.
- Existing-window foreground activation needs per-application validation.

## Official references

- [Copilot hardware key providers](https://learn.microsoft.com/en-us/windows/apps/develop/windows-integration/microsoft-copilot-key-provider)
- [Copilot state changes and fast path](https://learn.microsoft.com/en-us/windows/apps/develop/windows-integration/copilot-key-state)
- [Packaged desktop protocol activation](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/desktop-to-uwp-extensions#start-your-application-by-using-a-protocol)
- [ShellExecuteExW](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecuteexw)
- [IApplicationActivationManager](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-iapplicationactivationmanager-activateapplication)
- [SetForegroundWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow)
