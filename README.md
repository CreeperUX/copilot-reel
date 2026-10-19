# Copilot Reel

A native Windows application wheel that opens near the pointer when the Copilot key is held. Move into its center, choose an application, and release to open it. A short Copilot tap directly opens the saved preferred agent.

The native implementation builds and runs. Soft Orbit rendering, Windows appearance synchronization, configuration, single-instance activation, and launching are implemented. A signed development MSIX has been verified as eligible in Windows Settings. The key route remains user-controlled. Current build, test, installation, and remaining physical-input/display/performance evidence is recorded in [validation](docs/validation.md).

The current interface uses nested radial menus: configured agents, a + entry for app management, and Settings. Settings remains a wheel, including in-place language adjustment through left/right chevrons or A / D, preferred-agent selection, and system appearance. English is the default; Simplified Chinese is a native persistent option. VI v0.6 documents official app artwork in consistent containers and pointer-led reveal/reverse animation. Runtime validation is tracked separately from the visual specification.

## Behavior

- The official Windows provider contract supplies the approximately 300 ms hold threshold and start/stop events.
- The wheel opens without taking keyboard focus. Moving outside its center cancellation zone selects a direction.
- Releasing commits at most one target, starts the closing animation, and activates that target once.
- A short Copilot tap opens the preferred agent directly. Optional `Ctrl+Alt+R` opens a click-to-select preview.
- The + entry opens a manage wheel for 1-8 targets: executables, shortcuts, URIs, and packaged-app AUMIDs. Presets and app selection remain radial; a focused native editor handles text and launch fields.
- Recognized entries use verified official artwork with common tile size, optical bounds, and selection treatment. Unknown targets use a neutral app glyph. Vendor artwork keeps its identity and proportions; Windows accent colors the selection container.
- Opening extends a line from the actual pointer to the nearby wheel center, normally 168 DIP away. Edge/corner clamping preserves the complete circle and can lengthen that line. Center entry arms selection; release before entry cancels.
- Settings is another wheel. Language changes in place with the visible arrows or A / D. Back or Esc returns one radial level.
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

The application uses C++20, Win32, Windows Composition, Direct2D, and DirectWrite. Geometry and text render at the target DPI. Supported official SVG marks render as vectors; official PNGs cover sources without usable SVG or unsupported SVG features. PNG/PDF UI previews are reference material, never animation frames. State changes and compositor timelines drive rendering instead of a permanent application frame loop.

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
