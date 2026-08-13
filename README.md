# Peek

Peek is a tiny native Windows utility that answers three questions quickly:

- **Ghost** — what was that window or process that flashed past?
- **Unlock** — which process is using this file?
- **Focus** — what Windows UI element is under the pointer?

It is intentionally offline, account-free, telemetry-free and implemented with C++20 plus Windows system APIs. The first release targets Windows 10/11 x64 and produces one executable with a statically linked C/C++ runtime.

## 使用 / Usage

1. 运行 `Peek.exe`。程序默认打开“闪现 / Ghost”，关闭主窗口后仍驻留托盘。
2. 点击窗口右上角的 `EN` 或 `中文` 即时切换语言；也可以右键托盘图标，在“语言 / Language”中选择。Peek 首次运行跟随 Windows 显示语言，之后记住你的选择。
3. **闪现 / Ghost**：看到窗口一闪而过后按 `Ctrl+Shift+G`，查看最近的进程和顶层窗口事件；点击卡片可展开路径、命令行和父进程信息。
4. **解锁 / Unlock**：切换到解锁页，把文件拖入窗口，或点击“选择文件”；选中占用进程后可以打开程序所在位置或在确认后结束进程。
5. **探查 / Focus**：按 `Ctrl+Shift+F` 或点击“开始探查”，把鼠标移到目标控件上；Peek 会显示 HWND、进程、窗口类、尺寸和可用的 UI Automation 信息。按 `Esc` 退出。
6. 右键托盘图标并选择“退出 / Exit”可完全结束 Peek。

Language selection is available from the `中文 / EN` button in the window or the tray menu. On first run Peek follows the Windows display language and then remembers the selected language.

### 关闭与托盘 / Closing and tray

- 首次点击窗口右上角的 `X` 时，Peek 会询问“最小化到托盘”还是“退出 Peek”；勾选“记住我的选择”后，下次直接执行该操作。
- 单击托盘图标可重新打开 Peek。右键托盘图标可选择“打开 Peek”“设置”“语言”或“退出”。
- 在托盘菜单的“设置”中，可以随时改为“每次询问”“最小化到托盘”或“退出 Peek”。设置只保存在当前 Windows 用户的本机注册表中。
- The first click on `X` asks whether to minimize to the tray or exit. The choice can be remembered and changed later from tray **Settings**.
- Left-click the tray icon to reopen Peek. Right-click it for **Open Peek**, **Settings**, **Language**, and **Exit Peek**.

## Current v0.1 scope

- One 420 × 540 native Win32 window and one tray icon.
- `Ctrl+Shift+G` opens Ghost; `Ctrl+Shift+F` enters Focus.
- Ghost keeps the latest 100 process/window events in memory only.
- Unlock accepts a file picker and drag-and-drop, then uses Restart Manager to list affected processes. It can reveal an executable or terminate a process after confirmation. It never closes arbitrary handles.
- Focus highlights the item under the pointer and reports Win32 and UI Automation properties. `Esc` exits.
- One small close-behavior settings popup; no settings center, networking, database, updater, plug-in system or background log file.

## Build on Windows

Requirements: Visual Studio 2022 Build Tools with the Desktop C++ workload, and CMake 3.24+.

```powershell
./scripts/build-release.ps1
```

With the Visual Studio multi-config generator, the executable is written to `build/release/bin/Release/Peek.exe` (the build script also handles single-config generators). See [TESTING.md](TESTING.md) for the Windows acceptance run.

## Privacy and permissions

Peek does not transmit or persist captured data. It normally runs unelevated. Windows can deny the command line, path, UI Automation properties, or process termination for protected/elevated processes; the UI shows the information that Windows permits instead of requesting elevation automatically.

## Known first-release trade-offs

- WMI process trace events reliably preserve the PID/name/parent relationship after a short process exits, but command line and full path are best-effort because those must be queried while the process still exists. If WMI event subscriptions are denied by local policy, Peek falls back to a low-cost 250 ms PID snapshot while window hooks remain active. Visible flashes remain event-driven, but the fallback can miss a headless process whose entire lifetime falls between samples and omits slow command-line enrichment to keep capture responsive.
- Out-of-context WinEvent hooks observe the current interactive desktop; secure desktop and higher-integrity UI can be inaccessible.
- Restart Manager is safe and official but does not enumerate every kind of kernel handle. Drivers, memory maps and processes that Restart Manager cannot identify may still hold a file.

Architecture and API rationale are in [ARCHITECTURE.md](ARCHITECTURE.md).
