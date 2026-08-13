# Peek architecture

## Product boundary

Peek is a question-and-answer utility, not a general task manager or log viewer. Data is bounded in memory, primary actions fit in one window, and every module reuses the same process/window models.

## Components

| Component | Responsibility | Windows API |
| --- | --- | --- |
| `ProcessResolver` | Resolve PID to name, image path, command line, parent and icon source | Toolhelp, `OpenProcess`, `QueryFullProcessImageName`, WMI fallback |
| `EventStore` | Thread-safe bounded event history and process start/stop correlation | Standard C++ only |
| `GhostMonitor` | Capture process lifecycle and desktop window events | WMI `Win32_ProcessStartTrace` / `StopTrace`, `SetWinEventHook` |
| `UnlockService` | Find processes registered as users of a selected file | Restart Manager |
| `FocusInspector` | Inspect the pointer target and UIA element | `WindowFromPoint`, UI Automation |
| `IconCache` | Cache small process icons by executable path | Shell image extraction |
| `PrivilegeHelper` | Centralize access checks and safe process actions | process tokens, `TerminateProcess` |
| `MainWindow` | Tray, hotkeys, cards, tabs and shared process details | Win32, GDI, DWM |
| `OverlayWindow` | Non-activating inspection highlight | layered topmost Win32 window |

## Event flow

Ghost owns two capture sources. WMI callbacks enqueue process start/stop records. WinEvent callbacks immediately snapshot title, rectangle and PID because the originating window may already be gone. `EventStore` merges a stop event with a matching start event to compute lifetime and retains the newest 100 records. UI refresh is posted to the main window so capture threads never paint or block on presentation.

## Reliability choices

### Ghost

The first version prefers WMI trace event classes because polling can never guarantee seeing a process whose entire lifetime falls between samples. The process event itself contains PID, name and parent PID; full path and command line are enrichment and therefore explicitly best-effort. Some managed Windows installations deny the event subscription to standard users (`WBEM_E_ACCESS_DENIED`), so Peek degrades to a low-cost 250 ms `EnumProcesses` PID snapshot rather than disabling Ghost entirely. Only newly observed PIDs receive the more expensive path and parent enrichment, and the capture thread intentionally avoids synchronous WMI command-line queries. A headless process can still escape if its entire lifetime falls between samples. Window observation remains event-driven through an out-of-context hook, so visible flashes are still captured without injecting a DLL into other applications.

### Unlock

Restart Manager is used because Windows maintains the affected-application relationship and returns stable process identity (`PID` plus process start time). The first version intentionally omits remote handle closure and calls neither `RmShutdown` nor native handle-duplication APIs. Termination is an explicit, confirmed action against the selected PID.

### Focus

Win32 supplies the HWND/class/title/process boundary. UI Automation supplies framework-neutral control metadata for Win32, Chromium and other accessibility providers. UIA calls run on the inspection worker thread, and unavailable/transient elements fail soft.

## Threading

- UI/tray/hotkeys: primary STA thread.
- WMI process watcher: dedicated MTA worker with a blocking notification enumerator.
- Focus sampling/UIA: dedicated MTA worker, activated only during inspect mode.
- WinEvent callbacks: system callback threads; snapshot and enqueue only.

Shared state uses short mutex sections. Shutdown sets stop events, cancels blocking WMI calls by releasing the enumerator, unhooks WinEvent, joins workers, then destroys UI resources.

## Resource and security posture

- No background timer while idle except active Focus sampling.
- Exactly 100 Ghost records by default.
- Icons are lazily loaded and bounded.
- No automatic elevation and no debug privilege enablement.
- Protected/elevated processes may expose partial information.
- No file or registry persistence except optional future autostart/hotkey settings.

## Official references

- [Restart Manager functions](https://learn.microsoft.com/windows/win32/rstmgr/functions)
- [RmGetList](https://learn.microsoft.com/windows/win32/api/restartmanager/nf-restartmanager-rmgetlist)
- [SetWinEventHook](https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-setwineventhook)
- [Win32_ProcessStartTrace](https://learn.microsoft.com/previous-versions/windows/desktop/krnlprov/win32-processstarttrace)
- [UI Automation ElementFromPoint](https://learn.microsoft.com/windows/win32/api/uiautomationclient/nf-uiautomationclient-iuiautomation-elementfrompoint)
