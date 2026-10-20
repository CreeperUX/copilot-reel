# Copilot Reel

A native Windows application wheel that opens centered on the pointer when the Copilot key is held. Move toward an application and release to open it. A short Copilot tap directly opens the saved preferred local application.

The native implementation builds and runs. Soft Orbit rendering, Windows appearance synchronization, configuration, single-instance activation, and launching are implemented. A signed development MSIX has been verified as eligible in Windows Settings. The key route remains user-controlled. Current build, test, installation, and remaining physical-input/display/performance evidence is recorded in [validation](docs/validation.md).

The current interface uses nested radial menus: configured agents and one + entry. The + opens a combined hub for adding, managing, language, preferred-agent selection, and system appearance. EN/ZH and sun/moon icons convey state without fixed captions. Hovering an app or command shows its concise name in the center, without an instructional second line. English is the default; Simplified Chinese is a native persistent option. Version 0.3.4 (VI v0.9) brings the Windows 11 neutral plate with a flyout shadow, a concentric selection seat that slides along the orbit, unified official artwork (plated marks fill their tile; open marks share one optical size; system apps use their Shell icons), Segoe Fluent Icons commands, and drill-in page transitions in which the chosen tile becomes the back control. Design values live in one token file shared by the application and the VI. Runtime validation is tracked separately from the visual specification.

## Behavior

- The official Windows provider contract supplies the approximately 300 ms hold threshold and start/stop events.
- The wheel opens without taking keyboard focus. Moving outside its center cancellation zone selects a direction.
- Releasing commits at most one target, starts the closing animation, and activates that target once.
- A short Copilot tap opens the preferred local application. Installed-agent discovery resolves supported legacy website defaults to their native application. If no local target is available, the native application picker asks for an executable or shortcut; a tap never falls back to a website. Optional `Ctrl+Alt+R` opens a click-to-select preview.
- The + entry combines settings and management for 1-8 targets: executables, shortcuts, URIs, and packaged-app AUMIDs. Presets and app selection remain radial; a focused native editor handles text and launch fields.
- Recognized entries use verified official artwork, identified by their launch target, with one tile system: plated marks fill and are masked to the tile, open marks sit on a neutral tile at equal optical size, and Windows Settings, Notepad and custom apps use their own Shell icons. Unknown targets use a neutral app glyph. Vendor artwork is never recolored or redrawn; the Windows accent ramp colors only the selection seat and the preferred badge.
- Ordinary opening leaves the mouse at the center. Only boundary conflicts move the canvas inward; if that displacement puts the mouse outside the center zone, center entry arms selection. No connector or accent perimeter is drawn.
- The + hub stays radial. EN/ZH changes in place through the flanking arrows or A / D. The center chevron or Esc returns one level. Main has an empty idle center; hovered apps and commands show one concise name at the center.
- The Soft Orbit base follows Windows light/dark appearance. Transparency effects request a frosted backdrop when supported; otherwise the base is opaque. Selection uses the system accent. High contrast, disabled animations, and energy saver take precedence where applicable.

Targets inside the wheel do not need to register as Copilot providers. Only Copilot Reel registers with Windows.

## Build and preview

The two release architectures are Windows 11 x64 and Windows 11 ARM64. Both use the same source and the same Win32, Composition, Direct2D, and DirectWrite contracts. The package declares build 22621 as its minimum; this is not a claim that every Copilot hold-state feature has been verified on that build, and the ARM64 package has not been executed on ARM hardware.

```powershell
.\tools\bootstrap-toolchain.ps1
.\tools\build.ps1
.\tools\build.ps1 -Architecture arm64
.\build\Release\CopilotReel.exe --settings
.\build\Release\CopilotReel.exe --preview
```

Bootstrap downloads pinned Microsoft MSVC and Windows SDK packages for both architectures into the ignored `tools/local` directory, without installing an IDE or changing the machine PATH. The default build is x64 and produces `build/Release/CopilotReel.exe`. `-Architecture arm64` cross-compiles `build/arm64/Release/CopilotReel.exe` and its unit-test binaries, then checks that their PE machine is ARM64 (`0xAA64`). It does not run them on an x64 host. See [toolchain notes](docs/toolchain.md).

The application uses C++20, Win32, Windows Composition, Direct2D, and DirectWrite. Geometry and text render at the target DPI on the device-pixel grid; official raster artwork is pre-scaled per DPI with high-quality cubic filtering. `design/vi/tokens.json` is the single source of design values: the build generates `src/vi_tokens.h` from it, and the VI generator reads it directly. PNG/PDF UI previews are reference material, never animation frames. State changes and compositor timelines drive rendering instead of a permanent application frame loop.

## Packaging and Windows integration

```powershell
.\tools\package.ps1
.\tools\package.ps1 -Architecture arm64
.\tools\verify-registration.ps1
```

Packaging without a certificate produces an unsigned MSIX for inspection. The default package is `artifacts/CopilotReel_<version>_x64.msix`, staged in `artifacts/package`. `-Architecture arm64` stages `artifacts/package-arm64`, writes `ProcessorArchitecture="arm64"` into that package only, and leaves `packaging/AppxManifest.xml` as the x64 source manifest. To sign with an existing certificate whose subject matches the manifest publisher, pass `-CertificateThumbprint`. Neither packaging nor registration verification imports certificates or installs the package automatically. `tools/install-development.ps1` accepts only the x64 package, so an ARM64 package cannot replace the installed x64 app on this host. Development trust is machine-specific; the certificate authorized for this machine is recorded in local, ignored signing artifacts.

The package declares `com.microsoft.windows.copilotkeyprovider` and the private `copilot-reel:` protocol. Existing processes accept the HWND message contract; new instances forward commands to the resident process. Windows controls picker eligibility and the user's selected key binding. See [validation status](docs/validation.md) for demonstrated behavior and remaining checks.

## Documentation

- [Architecture](docs/architecture.md)
- [Provider research and evidence](docs/research.md)
- [Validation status](docs/validation.md)
- [Appearance and performance testing plan](docs/testing-plan.md)
- [Configuration and isolated tests](docs/configuration.md)
- [Native toolchain](docs/toolchain.md)
- [Soft Orbit visual identity](docs/visual-identity.md)
- [Read-only provider inventory](tools/inspect-copilot-providers.ps1)

Development documentation, code comments, diagnostics, and commit messages use English. User-facing localization is maintained separately.
