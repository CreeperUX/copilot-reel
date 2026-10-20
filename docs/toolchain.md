# Native Windows toolchain

The repository builds both release architectures from a local x64-hosted MSVC toolchain, without installing the Visual Studio IDE or changing the machine's PATH. The two packages are x64 and ARM64. Run from a PowerShell prompt at the repository root:

```powershell
.\tools\bootstrap-toolchain.ps1
.\tools\build.ps1
.\tools\build.ps1 -Architecture arm64
```

`.\tools\build.ps1` with no architecture argument keeps the x64 output at `build\Release`. The ARM64 cross-build writes `build\arm64\Release` and does not execute its binaries: this development host is x64 and cannot run ARM64 images. Bootstrap downloads the pinned x64 and ARM64 archives into the ignored `tools/local/downloads` directory. It extracts Microsoft compiler, linker, standard library, and Windows SDK files underneath `tools/local`. Re-running it verifies the archives and reuses identical extracted files. `tools/bootstrap-toolchain.ps1 -VerifyOnly` verifies cached archive hashes and checks the required compiler, SDK, library, and packaging files without downloading or extracting anything.

The pinned inputs are recorded in `tools/toolchain-manifest.json`:

| Component | Version | Source |
| --- | --- | --- |
| MSVC x64 compiler/linker | 19.44.35229 / 14.44.35229 | Microsoft Visual Studio 17 release channel VSIX payloads |
| MSVC ARM64 cross compiler/linker (x64 host) | 19.44.35229 / 14.44.35229 | The same channel, `HostX64.TargetARM64` |
| MSVC standard library/CRT | 14.44 toolset, x64 and ARM64 desktop plus Store payloads | The same Microsoft channel |
| Windows SDK headers and tools | NuGet 10.0.26100.8249, SDK directory 10.0.26100.0 | Microsoft.Windows.SDK.CPP |
| Windows SDK x64 import libraries/UCRT | NuGet 10.0.26100.8249 | Microsoft.Windows.SDK.CPP.x64 |
| Windows SDK ARM64 import libraries/UCRT | NuGet 10.0.26100.8249 | Microsoft.Windows.SDK.CPP.arm64 |

The SDK contains C++/WinRT projected headers, `rc.exe`, `makeappx.exe`, and `signtool.exe`. Headers and packaging tools are host tools and are shared by both targets. No separate C++/WinRT code generation step is required for the Windows API types used by this application. Each architecture's CRT Store payload is included because it supplies common desktop link inputs such as `oldnames.lib`; the application itself is compiled as a Win32 desktop application with the static CRT (`/MT`). ARM64 linking passes `/MACHINE:ARM64` and uses `sdk-arm64` import libraries. The resource compiler remains the x64 `rc.exe`.

Every archive has a pinned SHA-256. VSIX hashes are copied from the official Microsoft Visual Studio channel manifest; SDK hashes are pinned from the exact packages downloaded from the official NuGet endpoint. Bootstrap checks HTTPS origins, verifies bytes before extraction, and rejects archive paths outside the destination. It does not change registry values, certificate trust, key bindings, or installed applications. Build temporarily sets INCLUDE, LIB, and PATH for its own process and restores them when finished.

To update the compiler or SDK, update the exact URLs and hashes in the manifest together with the toolset paths in `tools/build.ps1`. Keep the compiler, C++ library headers, and runtime libraries within one compatible toolset. Downloaded development tools remain third-party dependencies and are not included in the application's package.

Validation on this development machine: bootstrap verifies every pinned archive's SHA-256 before extraction, including the ARM64 compiler, CRT, and SDK libraries added for the second release. The x64 compiler, resource compiler, and MSIX packager have valid Microsoft Authenticode signatures. The ARM64 cross compiler and linker (`bin\Hostx64\arm64\cl.exe` and `link.exe`) also verified as Microsoft-signed on 2026-10-20. The x64 application and its unit tests have been built and executed with this toolchain. The ARM64 application has been cross-compiled with it and has not been executed; the PE check and host refusal are recorded in [validation](validation.md).

## Planned performance tooling

Release currently uses optimization without a matching application PDB. The [testing plan](testing-plan.md) proposes an optimized symbol-bearing Profile/RelWithDebInfo configuration, preserving Release optimization/link settings and retaining the exact EXE, PDB, tokens and source identity for each measurement. This configuration is not implemented yet; an unoptimized Debug binary is not a substitute for Release performance evidence.

The reviewed external tools are the Windows WPR recorder, Microsoft WPA/WPAExporter, and the local `C:\dev\lpt03\trace-code-bridge` 0.2.0 repository with its .NET 8 / TraceEvent 3.2.8 ETL adapter. The bridge handles offline CPU locations only. Its current checkout has no commit to pin, so future runs must record a content-hashed snapshot and tool versions. These tools are development dependencies and are not bundled into the Copilot Reel application.
