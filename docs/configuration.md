# Application configuration and test isolation

The reel reads `%LOCALAPPDATA%\CopilotReel\apps.ini` by default. Choose the main wheel's **+** entry to open the combined hub; use its grid icon to manage 1-8 targets or its + icon to add. Existing targets, app actions, and the preset collection use nested wheels. Text and launch details open a focused native editor. Each entry requires a name and actual target; a preset whose application is not found opens the editor to request its local application rather than substituting a website. Supported launch methods are Shell (an executable, shortcut, or URI) and an installed application's AUMID. Shell entries can also specify arguments and a working directory.

Application configuration uses UTF-16 LE with a BOM and preserves Unicode text. Backslashes are escaped as `\\`; tabs are escaped as `\t`. Quotes and equals signs do not need escaping. The schema version is `Version=1`. Prefer the settings window over manual editing. Saving writes a temporary file in the same directory, flushes it, and then replaces the configuration. When loading detects corruption, it retains the original file and temporarily supplies the default entries.

## Interface language

English is the default interface language. Open **Main reel > +**, then click the small arrows flanking the **EN/ZH** tile, or press **A / D**, to switch between English and Simplified Chinese. This is an in-place radial adjustment, not a dropdown in a separate rectangular settings window. The change updates the EN/ZH icon, tray text, editor labels, and subsequent dialogs. Hovering the EN/ZH tile or either arrow shows Language at the center. No fixed language heading, caption beneath the tile, or keyboard hint is rendered inside the wheel. Stored entry names remain user data. Built-in Notepad and Windows Settings labels, including the legacy Chinese defaults, are localized only while drawing the UI; their stored names and launch identities are untouched. Explicit custom names remain unchanged. This release has no autostart preference.

Language preference is stored independently in `%LOCALAPPDATA%\CopilotReel\preferences.ini`:

```ini
[General]
Language=en-US
```

Both languages read the same `apps.ini` and `behavior.ini` in the active profile. Switching language writes only `preferences.ini`; it never selects another application list, icon set, target order, or preferred target. The installed MSIX and a standalone development executable may have separate Windows application-data scopes. That distinction is packaging, not language. Isolated tests explicitly override their scope with `COPILOT_REEL_DATA_DIR`. Diagnostics expose the active `dataPath` and `language`.

The supported persisted values are `en-US` and `zh-CN`. Missing or invalid values select English. The radial adjustment applies only after its atomic preference write succeeds. The command-only wheel disables IME composition so A/D work under a Chinese keyboard layout; native text editors keep their input method. Windows-owned dialog controls may follow the Windows display language.

## Preferred agent

Open **Main reel > + > Preferred agent** and choose a configured target. A short Copilot tap launches the preferred local application; a hold opens the reel. The preferred marker appears on the selected agent's tile. `behavior.ini` stores a versioned hash of launch kind, target, arguments, and working directory, independently of the language file. Renaming and reordering retain the preference; removing the preferred profile falls back to the first remaining entry.

Fresh defaults use up to six discovered installed native applications, followed by Notepad and Windows Settings. Only known ChatGPT and Claude base-URL default profiles can migrate automatically to their verified installed target; the configuration is backed up before that migration is saved. Arbitrary pages and custom URLs are not migrated. User-created web targets remain usable through an explicit wheel selection, but short tap does not launch HTTP/HTTPS destinations. If a preferred web entry cannot resolve to an installed local application, a native picker titled Choose a local application requests an `.exe` or `.lnk` file. Selecting a web entry as preferred uses the same picker. A successful choice replaces the selected launch profile; a short tap then opens that local target. Missing presets still use the focused editor for name and target details. Cancelling does not open a browser.

The shared localization API in `src/localization.h` provides `CurrentLanguage`, `SetLanguage`, `Tr`, `LoadLanguage`, and `SaveLanguage`. Call `LoadLanguage` once during startup. It sets the current language and returns it. `SaveLanguage` stores the supplied preference without changing the current session language. Source comments, identifiers, diagnostic details, configuration keys, and development documentation use English; product UI translations are explicit string pairs.

## Isolated test data

Set `COPILOT_REEL_DATA_DIR` only in the test process environment to override the directory containing `apps.ini`, `preferences.ini`, and `behavior.ini`. The value must be an absolute drive path, such as `C:\dev\copilot_reel\build\test-data`, or a complete UNC share path. Relative paths, drive-relative paths such as `C:temp`, and device namespaces are rejected. An invalid override never falls back to the current working directory or the user's real configuration. An unset or empty override uses the normal LocalAppData location.

Create a dedicated directory before starting the test process. Do not persist this variable through system environment settings, and do not point it at existing user configuration. An already-running single instance keeps its startup environment; launch the target instance itself with the isolated environment.

`tests/launch_probe.cpp` builds as a separate Windows-subsystem executable with a `wWinMain` entrypoint. It has no window and launches no other application. Each invocation appends one `launched` line to `launch-probe.txt` in the explicitly supplied test directory, allowing duplicate launches to be detected. Missing environment variables, invalid paths, or missing directories cause a nonzero exit without writing to the default configuration location.

`tests/settings_tests.cpp` tests parsing, escaping, and validation boundaries without invoking configuration-file or window APIs. Its Unicode fixture data intentionally includes non-English names and paths.
