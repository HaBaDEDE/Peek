# Windows acceptance test

Run all commands from a normal, unelevated PowerShell window first. Repeat protected-process checks elevated only when intentionally testing access boundaries.

## Automated smoke test

```powershell
./scripts/test-windows.ps1 -Exe ./build/release/bin/Release/Peek.exe
```

The script launches Peek, checks that it stays alive, creates a short PowerShell process, and creates a temporary file lock for the manual Unlock step.

## Scoop package smoke test

The self-hosted `bucket/peek.json` manifest was tested on 2026-08-14 in an isolated Scoop environment:

- Added `https://github.com/HaBaDEDE/Peek` as the `peek` bucket.
- Installed `peek` version `0.1.0` from its versioned GitHub Release URL.
- Verified Scoop's SHA-256 check and the installed executable hash.
- Launched the installed executable and confirmed that the process remained alive.
- Uninstalled the package and verified removal of the shortcut and current-version link.

The isolated Scoop environment and test shortcut were removed after the test.

## Manual acceptance checklist

1. **Tray** — launch Peek, close the main window, and verify the tray icon remains. Double-click it and verify the window returns.
2. **Ghost hotkey** — run `powershell -NoProfile -Command "exit"`, press `Ctrl+Shift+G`, and verify the newest card identifies PowerShell with a short lifetime and a parent process.
3. **Window Ghost** — open and close Notepad quickly; verify create/show/hide/destroy records appear without turning the UI into a scrolling log.
4. **Unlock picker** — while the test script holds its temporary file open, choose that file in Unlock and verify `powershell.exe` is reported. Release the lock and refresh; verify it disappears.
5. **Unlock drag/drop** — repeat by dropping the file on the window.
6. **Focus Win32** — inspect Notepad edit area and verify HWND, class, title, rectangle and owning process.
7. **Focus Explorer** — inspect the Explorer address bar and file list; verify UIA Name/ControlType/AutomationId when the provider exposes them.
8. **Focus Chromium** — inspect a Chromium tab, address bar and page control; verify the highlight tracks and UIA failures remain nonfatal.
9. **Exit** — use the tray Exit command and confirm the process and hooks are removed.
10. **Language** — use the top-right `中文 / EN` button to switch both ways, verify the current page repaints immediately, then restart Peek and verify the selected language is remembered. Repeat from the tray Language submenu.

Record working set after 30 seconds idle with:

```powershell
Get-Process Peek | Select-Object Id, WorkingSet64, PrivateMemorySize64, CPU
```
