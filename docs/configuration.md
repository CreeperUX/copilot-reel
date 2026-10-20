# Application configuration and test isolation

The reel reads `%LOCALAPPDATA%\CopilotReel\apps.ini` by default. Choose the main wheel's **+** entry to open the combined hub; use its grid icon to manage 1-8 targets or its + icon to add. Existing targets, app actions, and the preset collection use nested wheels. Text and launch details open a focused native editor. Each entry requires a name and actual target; a preset whose application is not found opens the editor rather than silently changing the intended destination. Supported launch methods are Shell (an executable, shortcut, or URI) and an installed application's AUMID. Shell entries can also specify arguments and a working directory.

Application configuration uses UTF-16 LE with a BOM and preserves Unicode text. Backslashes are escaped as `\\`; tabs are escaped as `\t`. Quotes and equals signs do not need escaping. The schema version is `Version=1`. Prefer the settings window over manual editing. Saving writes a temporary file in the same directory, flushes it, and then replaces the configuration. When loading detects corruption, it retains the original file and temporarily supplies the default entries.

## Interface language

English is the default interface language. Open **Main reel > +**, then click the small arrows flanking the **EN/CN** tile, or press **A / D**, to switch between English and Simplified Chinese. This is an in-place radial adjustment, not a dropdown in a separate rectangular settings window. The change updates the EN/CN icon, tray text, editor labels, and subsequent dialogs. No language heading, caption, or keyboard hint is rendered inside the wheel. Entry names are user data and are never translated or renamed. This release has no autostart preference.

Language preference is stored independently in `%LOCALAPPDATA%\CopilotReel\preferences.ini`:

```ini
[General]
Language=en-US
```

The supported persisted values are `en-US` and `zh-CN`. Missing or invalid values select English. The radial adjustment applies only after its atomic preference write succeeds. The command-only wheel disables IME composition so A/D work under a Chinese keyboard layout; native text editors keep their input method. Windows-owned dialog controls may follow the Windows display language.

## Preferred agent

Open **Main reel > + > Preferred agent** and choose a configured target. A short Copilot tap launches that target directly; a hold opens the reel. The preferred marker appears on the selected agent's tile. `behavior.ini` stores a versioned hash of launch kind, target, arguments, and working directory, independently of the language file. Renaming and reordering retain the preference; removing the preferred profile falls back to the first remaining entry.

The shared localization API in `src/localization.h` provides `CurrentLanguage`, `SetLanguage`, `Tr`, `LoadLanguage`, and `SaveLanguage`. Call `LoadLanguage` once during startup. It sets the current language and returns it. `SaveLanguage` stores the supplied preference without changing the current session language. Source comments, identifiers, diagnostic details, configuration keys, and development documentation use English; product UI translations are explicit string pairs.

## Isolated test data

Set `COPILOT_REEL_DATA_DIR` only in the test process environment to override the directory containing `apps.ini`, `preferences.ini`, and `behavior.ini`. The value must be an absolute drive path, such as `C:\dev\copilot_reel\build\test-data`, or a complete UNC share path. Relative paths, drive-relative paths such as `C:temp`, and device namespaces are rejected. An invalid override never falls back to the current working directory or the user's real configuration. An unset or empty override uses the normal LocalAppData location.

Create a dedicated directory before starting the test process. Do not persist this variable through system environment settings, and do not point it at existing user configuration. An already-running single instance keeps its startup environment; launch the target instance itself with the isolated environment.

`tests/launch_probe.cpp` builds as a separate Windows-subsystem executable with a `wWinMain` entrypoint. It has no window and launches no other application. Each invocation appends one `launched` line to `launch-probe.txt` in the explicitly supplied test directory, allowing duplicate launches to be detected. Missing environment variables, invalid paths, or missing directories cause a nonzero exit without writing to the default configuration location.

`tests/settings_tests.cpp` tests parsing, escaping, and validation boundaries without invoking configuration-file or window APIs. Its Unicode fixture data intentionally includes non-English names and paths.
