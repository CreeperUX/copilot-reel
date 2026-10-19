# Copilot Reel

A native Windows application wheel that appears around the pointer when the Copilot key is held. Move toward an application and release to open it; release in the center to cancel.

The first native prototype builds and runs. Soft Orbit rendering, Windows appearance synchronization, application configuration, single-instance activation, and launching are implemented. Core tests, 48 configuration checks, and the automated controller-to-launcher integration flow have passed. A signed development MSIX is installed and Windows Settings has been verified to recognize Copilot Reel. The key route remains a user-controlled setting. Physical Copilot-key delivery and the remaining display/performance matrix are still being validated.

The interface defaults to English and offers a native Simplified Chinese switch with persistent preferences. Ordinary launch opens the pointer-centered reel; the functional settings editor is an explicit action. VI v0.4 specifies a compact OSD replacement with second-level Settings and guarded full-circle edge placement. Those new OSD screens and edge behavior are not implemented yet.

## Behavior

- The official Windows provider contract supplies the approximately 300 ms hold threshold and start/stop events.
- The wheel opens without taking keyboard focus. Moving outside its center cancellation zone selects a direction.
- Releasing commits at most one target and hides the wheel before activation.
- A short tap or optional `Ctrl+Alt+R` opens a click-to-select preview. Right-click cancels.
- The native settings window manages 1–8 targets: executables, shortcuts, URIs, and applicable packaged-app AUMIDs.
- The Soft Orbit base follows Windows light/dark appearance. Transparency effects request a frosted backdrop when supported; otherwise the base is opaque. Selection uses the system accent. High contrast, disabled animations, and energy saver take precedence where applicable.

Targets inside the wheel do not need to register as Copilot providers. Only Copilot Reel registers with Windows.

## Build and preview

The development target is Windows 11 x64. The package declares build 22621 as its minimum; this is not a claim that every Copilot hold-state feature has been verified on that build.

```powershell
.\tools\bootstrap-toolchain.ps1
.\tools\build.ps1
.\build\Release\CopilotReel.exe --settings
.\build\Release\CopilotReel.exe --preview
```

Bootstrap downloads pinned Microsoft MSVC and Windows SDK packages into the ignored `tools/local` directory, without installing an IDE or changing the machine PATH. Build runs core/configuration tests and produces `build/Release/CopilotReel.exe`. See [toolchain notes](docs/toolchain.md).

The application uses C++20, Win32, Windows Composition, Direct2D, and DirectWrite. Geometry and text render at the target DPI; PNG/PDF design previews are reference material, not animation frames. State changes and compositor timelines drive rendering instead of a permanent application frame loop.

## Packaging and Windows integration

```powershell
.\tools\package.ps1
.\tools\verify-registration.ps1
```

Packaging without a certificate produces an unsigned MSIX for inspection. To sign with an existing certificate whose subject matches the manifest publisher, pass `-CertificateThumbprint`. Neither packaging nor registration verification imports certificates or installs the package automatically. Development trust is machine-specific; the certificate authorized for this machine is recorded in local, ignored signing artifacts.

The package declares `com.microsoft.windows.copilotkeyprovider` and the private `copilot-reel:` protocol. Existing processes accept the HWND message contract; new instances forward commands to the resident process. Windows controls picker eligibility and the user's selected key binding. See [validation status](docs/validation.md) for demonstrated behavior and remaining checks.

## Documentation

- [Architecture](docs/architecture.md)
- [Provider research and evidence](docs/research.md)
- [Validation status](docs/validation.md)
- [Configuration and isolated tests](docs/configuration.md)
- [Native toolchain](docs/toolchain.md)
- [Soft Orbit visual identity](docs/visual-identity.md)
- [Read-only provider inventory](tools/inspect-copilot-providers.ps1)

Development documentation, code comments, diagnostics, and commit messages use English. User-facing localization is maintained separately.
