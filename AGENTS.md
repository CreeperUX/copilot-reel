# Agent handoff

Copilot Reel is developed by several coding agents in turn. Before starting, read this file, `git log` and `git status`. Before handing over, add your session and what it changed. The documents in `docs/` remain the product and design contract; this file records who did what and the rules every agent follows.

## Rules for every agent

- Development artifacts are English: code, comments, script output, docs, design tokens, VI and commit messages. Replies to the user are in Chinese.
- The product UI defaults to English, with Simplified Chinese (`ZH`) as the option. Both languages share one profile: switching language never changes apps, order, icons or the preferred agent.
- Every new release on this machine: back up the configuration, uninstall the old package, install the new one, and restore the configuration (`tools/install-development.ps1 -PackagePath <signed msix>`).
- Commits use the existing author, `baka_sayaka <60833924+bakasayaka@users.noreply.github.com>`, passed per command. Never change git config. Commit or push only when the user asks.
- Interaction and visual checks run through the test seam: `tests/run-reel-seam.py` and `tools/capture-seam.py` drive a separate unpackaged test instance through its own channel. They never move the pointer, send input or take the foreground, and the installed reel keeps running.
- `tests/run-radial.py`, `tests/run-language-invariance.py` and `tools/capture-v034.py` are 0.3.4 evidence. They drive the real pointer, need the installed instance to quit first and must not run while the user works. Restart the installed reel afterwards with `tools/open-installed.ps1 -View Background`.
- Settled decisions; do not reopen them without the user:
  - Soft Orbit with one plate.
  - A quiet center: one name on hover, no titles or captions.
  - EN/ZH and sun/moon show state in the icon itself.
  - One merged `+` entry.
  - Official vendor artwork with one tile treatment.
  - The reel opens centered on the pointer and moves only as far as needed at screen edges, with center gating.
  - No line, perimeter-sweep or accent-stroke opening animation.
  - While the reel is closed, a short tap opens the preferred local agent, never a web page; a hold opens the reel.
  - Inside an open menu every input confirms, goes back or adjusts: a tap, or a new hold and its release, confirms the selected item (approved 2026-10-21).
  - The reel starts in the background at sign-in through a packaged startup task (approved 2026-10-21).

## Sessions

| Tool | Session ID | Transcript |
| --- | --- | --- |
| Codex | `01a15515-8bd6-7c13-b974-98614ac37e54` | `%USERPROFILE%\.codex\sessions\2026\10\19\rollout-2026-10-19T18-53-57-01a15515-8bd6-7c13-b974-98614ac37e54.jsonl` |
| Claude Code | `f4bd2732-bc2e-40e3-bdc3-52ecd97171f3` | `%USERPROFILE%\.claude\projects\c--dev-copilot-reel\f4bd2732-bc2e-40e3-bdc3-52ecd97171f3.jsonl` |
| Cursor | `dc23e9cc-f13b-4052-819b-5fa60d746ad3` | `%USERPROFILE%\.cursor\projects\c-dev-copilot-reel\agent-transcripts\dc23e9cc-f13b-4052-819b-5fa60d746ad3\` |

### Codex: product, platform and interaction

Role: platform research, product behavior, input and launch logic, and test strategy. Codex started the project and owns how the reel behaves.

Work, 2026-10-19 to 2026-10-20:
- Reverse-engineered how Windows Settings lists Copilot key providers (REA and Ghidra on `SettingsHandlers_nt.dll`, 184 package manifests). It chose the official provider contract with the roughly 300 ms hold over a keyboard hook (`docs/research.md`).
- Created the repository. Built the native C++20 Win32 provider with the URI and HWND fast paths, MSIX packaging and the development signing certificate, and confirmed that Windows Settings lists Copilot Reel.
- Took the product from the first Soft Orbit prototype to 0.3.3, and the VI from v0.1 to v0.8:
  - Pointer-centered placement with minimal edge clamping.
  - The radial settings hub behind one `+`.
  - Official agent presets.
  - Short tap and hold behavior, with local-first launching.
  - EN/ZH localization with pixel-identical icons.
- Wrote `docs/testing-plan.md`: appearance isolation from the live system, reuse of the ETW/WPR workspace for performance, and a proposed unified Confirm / Back / Adjust interaction model based on ASUS OSD and macOS menus. None of it is implemented yet.
- Commits: `b44c275`, `bde6c85`, `cf9eb0c`, `4c1341d`, `dc4da31`, `a298c96`, `2f9e1f2`, `92966d7`.

### Claude Code: visual design, rendering and motion

Role: art direction and its implementation (VI, design tokens, renderer, iconography, motion), code review, and local configuration on this machine.

Work, 2026-10-20:
- Audited the VI against the code and rebuilt the renderer for 0.3.4:
  - a layered composition tree on Windows neutral plates with one flyout shadow and an acrylic graph;
  - a selection seat that travels along the orbit;
  - drill-in/out transitions in which the tapped tile becomes the back chevron;
  - plated, glyph and command artwork classes identified by launch target;
  - the live accent ramp and a YaHei UI fallback for Chinese text.
- Made `design/vi/tokens.json` the single source of design values. `tools/gen-vi-tokens.ps1` generates `src/vi_tokens.h` from it, and the VI generator reads it directly. Published VI v0.9 (15 pages).
- Added native capture tooling (`tools/capture-v034.py`, `capture-window.exe`). Fixed the findings of an independent code review, passed the regression, and installed 0.3.4.
- Replaced Gemini, Perplexity and Notepad on this machine's reel with Cursor, ZCode and DeepSeek Harness, added Claude Science after Claude (see [This machine](#this-machine)), and wrote this file.
- Released 0.3.4.1 as the prerelease `v0.3.4.1-beta` and installed it here:
  - the reel passes on its foreground right before a launch, so an already running agent should come forward instead of flashing in the taskbar;
  - an installed Claude Science shows its own mark instead of the Claude mark;
  - `tools/package.ps1` packages x64 again; the ARM64 change had made it refuse every x64 package.
- Commits: `47a63e9`, `2bc3534`, `d905a41`, `eac04da`.

Work, 2026-10-21 to 2026-10-22 (0.3.5, released as the prerelease `v0.3.5-beta` and installed here):
- Traced two field defects to the Shell (`twinui.pcshell.dll`): it starts a provider that is not running with no arguments, and minimizes the provider window after a fast-path tap. Argument-less starts are now classified (startup task, held key, ready resident, cold tap), a packaged startup task keeps the reel resident from sign-in, and the reel ignores minimize commands.
- Implemented the unified Confirm / Back / Adjust model the user approved: `ReelInput` (`src/core.h`) and `RouteKey` (`src/radial_menu.h`).
- Local agent discovery (`src/agent_catalog.h`, `src/presets.cpp`, `InstalledIdentity`): catalog rules by executable and install folder from Start-menu shortcuts, Uninstall keys and folder probes, on a worker thread. Add lists installed agents not yet in the reel first, one tap adds them.
- Reorder from an app's actions page with per-side chevrons (`src/renderer.cpp`) or A/D.
- A running desktop agent comes forward instead of starting again (`ActivateRunning`).
- The test-input seam (`src/test_input.*`), `tests/run-reel-seam.py` and `tools/capture-seam.py`: interaction and pixel checks without touching the pointer, input, focus or the installed reel.
- A five-lens review with adversarial verification; all confirmed findings fixed. Evidence and limits are in `docs/validation.md` under v0.3.5.

### Cursor: build, release and distribution

Role: toolchain, multi-architecture builds, signing, the GitHub remote and releases.

Work, 2026-10-20:
- Added an ARM64 release that is cross-compiled from the same source with the pinned MSVC 14.44 and the Windows SDK 10.0.26100 ARM64 packages:
  - `tools/build.ps1 -Architecture arm64` and `tools/package.ps1 -Architecture arm64` produce it, and `tools/toolchain-manifest.json` pins the toolchain;
  - `tools/install-development.ps1` accepts only x64 packages;
  - ARM64 binaries are checked for PE machine `0xAA64` and never run on this x64 host.
- Ran the x64 regression: unit tests, 21/21 radial groups, and 0 differing pixels between languages. Created the private repository <https://github.com/bakasayaka/copilot_reel> and pushed `main`.
- Published the prerelease `v0.3.4-beta`. It carries both packages, signed with the development certificate `CN=CopilotReel Development` (thumbprint `061ED5990CEBC4F2D7C252ABE3EFE7EB99D6239F`). The ARM64 package was signed after the release was created and replaced on it. The matching `docs/validation.md` update was left uncommitted.
- Commit: `b6fb4e7`.

## Current state

- 0.3.5.0 x64 is installed on this machine and runs in the background; its startup task is enabled. The Copilot key is bound to it. GitHub carries the prereleases `v0.3.4-beta`, `v0.3.4.1-beta` and `v0.3.5-beta`, each with signed x64 and ARM64 packages.
- `main` tracks `origin/main` on GitHub. The ARM64 package has never run on ARM hardware.
- Open validation items are listed in `docs/validation.md`. For 0.3.5 the physical Copilot key still has to confirm: tap and hold inside an open menu, a cold start, the start after sign-in, and a running Cursor or DeepSeek Harness coming forward.
- VI v0.9 is not regenerated for 0.3.5: it has no App actions scene (`design/vi/source/reel.py` still names `languageControl`) and draws the Claude Science preset with the Claude mark. The tokens already describe the reorder chevrons.

## Open product questions

- Start launch while the reel is not running: it cannot be told apart from the Shell's argument-less fallback, so it opens the preferred agent. A hidden provider entry (`AppListEntry="none"`) plus a visible Start entry with `--preview` would separate them; check first that the Settings Copilot-key picker still lists the provider.
- Launcher stubs (Claude Science's window runs from `%USERPROFILE%\.claude-science\electron\<version>\`) are started again instead of brought forward; matching them needs a per-rule runtime image in `src/agent_catalog.h`.
- Catalog rules confirmed only from vendor docs or third-party sources: Windsurf/Devin, Kiro, Trae, Trae CN, Antigravity, JetBrains Toolbox folder names. JetBrains Air and Junie are not discovered.

## This machine

The installed reel is configured in `%LOCALAPPDATA%\Packages\CopilotReel.Desktop_nks6vs6sjb1qw\LocalCache\Local\CopilotReel\`. Its order is:

1. ChatGPT (preferred)
2. Claude
3. Claude Science
4. Cursor
5. ZCode
6. DeepSeek Harness
7. Windows Settings
8. `+`

Claude Science, Cursor, ZCode and DeepSeek Harness point at their executables under `%LOCALAPPDATA%\Programs`. The configuration from before these changes, with web targets for Gemini and Perplexity and with Notepad, is backed up in `artifacts/backups/20261020-210309`.
