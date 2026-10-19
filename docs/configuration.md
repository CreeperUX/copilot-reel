# Application configuration and test isolation

The reel reads `%LOCALAPPDATA%\CopilotReel\apps.ini` by default. Open the OSD from the tray or `--settings`, then choose **Apps** to manage 1-8 entries. Each entry requires a name and target. The focused editor keeps these common fields together; **Advanced** opens a separate compact sheet for launch method, arguments, and working directory. Supported launch methods are Shell (an executable, shortcut, or URI) and an installed application's AUMID. Shell entries can also specify a working directory. Back and Esc preserve the draft between the editor and Advanced; leaving an unsaved app draft asks before discarding it.

Application configuration uses UTF-16 LE with a BOM and preserves Unicode text. Backslashes are escaped as `\\`; tabs are escaped as `\t`. Quotes and equals signs do not need escaping. The schema version is `Version=1`. Prefer the settings window over manual editing. Saving writes a temporary file in the same directory, flushes it, and then replaces the configuration. When loading detects corruption, it retains the original file and temporarily supplies the default entries.

## Interface language

English is the default interface language. Open **Quick menu > Settings > Language** to switch the native combo between English and Simplified Chinese. The change updates window labels, launch-method descriptions, status messages, and subsequent dialogs. Changing the language notifies the controller so it can refresh the reel and tray UI. Entry names are user data and are never translated or renamed. Built-in default entry names remain `Notepad` and `Windows Settings`. **About Copilot Reel** shares the secondary Settings page; this release has no autostart preference.

Language preference is stored independently in `%LOCALAPPDATA%\CopilotReel\preferences.ini`:

```ini
[General]
Language=en-US
```

The supported persisted values are `en-US` and `zh-CN`. Missing or invalid values select English. A language switch applies to the current session even when the preference cannot be saved; settings reports the persistence failure. Windows-owned dialog controls may follow the Windows display language.

The shared localization API in `src/localization.h` provides `CurrentLanguage`, `SetLanguage`, `Tr`, `LoadLanguage`, and `SaveLanguage`. Call `LoadLanguage` once during startup. It sets the current language and returns it. `SaveLanguage` stores the supplied preference without changing the current session language. Source comments, identifiers, diagnostic details, configuration keys, and development documentation use English; product UI translations are explicit string pairs.

## Isolated test data

Set `COPILOT_REEL_DATA_DIR` only in the test process environment to override the directory containing both `apps.ini` and `preferences.ini`. The value must be an absolute drive path, such as `C:\dev\copilot_reel\build\test-data`, or a complete UNC share path. Relative paths, drive-relative paths such as `C:temp`, and device namespaces are rejected. An invalid override never falls back to the current working directory or the user's real configuration. An unset or empty override uses the normal LocalAppData location.

Create a dedicated directory before starting the test process. Do not persist this variable through system environment settings, and do not point it at existing user configuration. An already-running single instance keeps its startup environment; launch the target instance itself with the isolated environment.

`tests/launch_probe.cpp` builds as a separate Windows-subsystem executable with a `wWinMain` entrypoint. It has no window and launches no other application. Each invocation appends one `launched` line to `launch-probe.txt` in the explicitly supplied test directory, allowing duplicate launches to be detected. Missing environment variables, invalid paths, or missing directories cause a nonzero exit without writing to the default configuration location.

`tests/settings_tests.cpp` tests parsing, escaping, and validation boundaries without invoking configuration-file or window APIs. Its Unicode fixture data intentionally includes non-English names and paths.
