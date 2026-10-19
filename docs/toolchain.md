# Native Windows toolchain

The repository can build with a local x64 MSVC toolchain, without installing the Visual Studio IDE or changing the machine's PATH. Run from a PowerShell prompt at the repository root:

```powershell
.\tools\bootstrap-toolchain.ps1
.\tools\build.ps1
```

Bootstrap downloads approximately 300 MiB into the ignored `tools/local/downloads` directory. It extracts Microsoft compiler, linker, standard library, and Windows SDK files underneath `tools/local`. Re-running it verifies the archives and reuses identical extracted files. `tools/bootstrap-toolchain.ps1 -VerifyOnly` verifies cached archive hashes and checks the required compiler, SDK, library, and packaging files without downloading or extracting anything.

The pinned inputs are recorded in `tools/toolchain-manifest.json`:

| Component | Version | Source |
| --- | --- | --- |
| MSVC x64 compiler/linker | 19.44.35229 / 14.44.35229 | Microsoft Visual Studio 17 release channel VSIX payloads |
| MSVC standard library/CRT | 14.44 toolset, serviced headers/libraries | The same Microsoft channel |
| Windows SDK headers and tools | NuGet 10.0.26100.8249, SDK directory 10.0.26100.0 | Microsoft.Windows.SDK.CPP |
| Windows SDK x64 import libraries/UCRT | NuGet 10.0.26100.8249 | Microsoft.Windows.SDK.CPP.x64 |

The SDK contains C++/WinRT projected headers, `rc.exe`, `makeappx.exe`, and `signtool.exe`. No separate C++/WinRT code generation step is required for the Windows API types used by this application. The CRT Store payload is included because it supplies common desktop link inputs such as `oldnames.lib`; the application itself is compiled as a Win32 desktop application with the static CRT (`/MT`).

Every archive has a pinned SHA-256. VSIX hashes are copied from the official Microsoft Visual Studio channel manifest; SDK hashes are pinned from the exact packages downloaded from the official NuGet endpoint. Bootstrap checks HTTPS origins, verifies bytes before extraction, and rejects archive paths outside the destination. It does not change registry values, certificate trust, key bindings, or installed applications. Build temporarily sets INCLUDE, LIB, and PATH for its own process and restores them when finished.

To update the compiler or SDK, update the exact URLs and hashes in the manifest together with the toolset paths in `tools/build.ps1`. Keep the compiler, C++ library headers, and runtime libraries within one compatible toolset. Downloaded development tools remain third-party dependencies and are not included in the application's package.

Validation on this development machine: all seven pinned archives passed SHA-256 checks; all extraction paths were exercised; the compiler, resource compiler, and MSIX packager have valid Microsoft Authenticode signatures. The current application and core tests have been built using this toolchain.
