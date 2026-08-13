<a id="top"></a>

<div align="center">

# Peek

### 三个问题，一个极小的 Windows 工具

**刚才闪过去的是什么？ · 谁占用了这个文件？ · 鼠标下面是什么控件？**

`原生 Win32`　`单文件 EXE`　`离线运行`　`无账户`　`无遥测`　`中英双语`

[简体中文](#readme-zh) · [English](#readme-en)

</div>

---

<a id="readme-zh"></a>

## 简体中文

Peek 是一个轻量、快速的 Windows 系统观察工具。它不是任务管理器，也不试图取代 PowerToys；它只专注于三个日常中很难立刻回答的问题。

| 功能 | 它回答的问题 | 适合什么时候使用 |
| --- | --- | --- |
| **闪现 Ghost** | 刚才一闪而过的窗口或程序是什么？ | 黑框、PowerShell、安装器或更新程序突然出现又消失 |
| **解锁 Unlock** | 谁正在占用这个文件？ | 文件无法删除、移动、重命名或覆盖 |
| **探查 Focus** | 鼠标下面的 Windows UI 到底是什么？ | 想知道窗口、按钮、输入框属于哪个程序或控件 |

### 为什么是 Peek

- **极小而直接**：三个功能集中在一个约 `420 × 540` 的窗口中，没有欢迎页、侧边栏和复杂设置中心。
- **真正原生**：使用 C++20、Win32 API 和 Windows 系统组件，不使用 Electron、WebView、Qt 或 .NET Runtime。
- **默认离线**：没有账户、联网、云同步、在线数据库或遥测。
- **随时可用**：常驻系统托盘，通过全局快捷键快速打开，不需要事先配置。
- **中英双语**：首次启动跟随 Windows 显示语言，此后记住手动选择的语言。

## 下载和启动

项目中的 [dist/Peek.exe](dist/Peek.exe) 是已经构建好的 Windows x64 程序。

1. 下载 `Peek.exe`。
2. 双击运行，不需要安装。
3. Peek 启动后会显示主窗口，并在任务栏右下角创建托盘图标。
4. 如果 Windows SmartScreen 提示未知发布者，请先确认文件来自本仓库；当前版本尚未进行商业代码签名。

> 系统要求：Windows 10 或 Windows 11，x64。

## 功能详解

### 闪现 Ghost

> 回答：“刚才闪过去的到底是什么？”

Ghost 在后台观察短生命周期的进程和顶层窗口事件。当命令行窗口、脚本宿主、更新程序或安装器很快出现又消失时，它会保留最近的事件，方便你事后查看。

每条记录会尽量显示：

- 发生时间、程序名和 PID
- 进程启动、退出以及窗口创建、显示、隐藏、销毁事件
- 进程存活时间或窗口存在时间
- EXE 路径和命令行
- 父进程、父进程路径
- 窗口标题

使用方法：

1. 看到窗口一闪而过后，按 `Ctrl + Shift + G`。
2. Peek 会打开 Ghost，并把最近发生的事件放在最前面。
3. 点击记录可展开路径、命令行和父进程等详细信息。
4. 可从记录中打开程序所在位置，或直接进入 Focus 探查模式。

Ghost 只在内存中保留最近约 100 条事件，不会创建后台日志文件。进程已经退出后，路径和命令行可能来不及读取；如果 WMI 订阅被系统策略阻止，Peek 会自动切换到低开销的 250 ms 进程快照模式。

### 解锁 Unlock

> 回答：“谁正在占用这个文件？”

当 Windows 提示文件正在使用时，Unlock 可以找出可能的占用进程。

使用方法：

1. 切换到 **解锁 / Unlock**。
2. 把文件拖进 Peek，或点击“选择文件”。
3. 查看占用程序的名称、PID 和 EXE 路径。
4. 根据需要打开程序所在位置，或者在确认提示后结束进程。

Peek 使用 Windows 官方 Restart Manager 查询占用者。第一版不会强制关闭任意 Handle，以避免文件损坏和系统不稳定。驱动程序、内存映射或 Restart Manager 无法识别的内核占用，可能不会出现在结果中。

> 结束进程可能导致未保存的数据丢失。请先尝试正常关闭对应程序。

### 探查 Focus

> 回答：“鼠标下面这个 Windows UI 到底是什么？”

Focus 是一个面向普通用户的轻量 UI 探针。它会高亮鼠标下方的窗口或控件，并显示能够读取到的 Win32 与 UI Automation 信息。

使用方法：

1. 按 `Ctrl + Shift + F`，或在 Focus 页点击“开始探查”。
2. 将鼠标移动到目标窗口、按钮、输入框或其他控件上。
3. 查看高亮区域和实时属性。
4. 按 `Esc` 结束探查。

可显示的信息包括：

- HWND、进程名称、PID 和 EXE 路径
- Window Class、Window Title
- 坐标、宽度和高度
- AutomationId、ControlType、Name、Value
- UI Automation BoundingRectangle

某些管理员窗口、受保护窗口、安全桌面或未实现 UI Automation Provider 的程序，只会返回部分信息，这是 Windows 权限和目标程序能力造成的正常限制。

## 快捷键和托盘

| 操作 | 方法 |
| --- | --- |
| 打开最近的闪现记录 | `Ctrl + Shift + G` |
| 进入 Focus 探查模式 | `Ctrl + Shift + F` |
| 退出 Focus | `Esc` |
| 重新打开主窗口 | 单击或双击托盘图标 |
| 打开托盘菜单 | 右键托盘图标 |
| 完全退出 Peek | 托盘菜单 → “退出” |

右键托盘图标可以选择：

- 打开 Peek
- 设置
- 语言：简体中文 / English
- 退出 Peek

## 切换语言

Peek 内置简体中文和 English，无需下载额外语言包。

- 在主窗口右上角点击 `中文 / EN`，立即切换界面语言。
- 或右键托盘图标，打开“语言 / Language”子菜单进行选择。
- 第一次运行时自动跟随 Windows 显示语言。
- 手动切换后会记住选择，下次启动继续使用。

README 语言也可以通过页面顶部的 [简体中文](#readme-zh) / [English](#readme-en) 快速跳转。

## 关闭窗口与设置

第一次点击窗口右上角的 `X` 时，Peek 会询问：

- **最小化到托盘**：隐藏主窗口，但继续捕获 Ghost 事件并响应快捷键。
- **退出 Peek**：完全结束程序、后台监听和全局快捷键。

勾选“记住我的选择”后，以后会直接执行该操作。你可以随时右键托盘图标并打开“设置”，改为：

- 每次询问
- 最小化到托盘
- 退出 Peek

语言和关闭行为只保存在当前 Windows 用户的本机注册表中。

## 隐私与权限

- 不联网，不上传文件、进程或窗口信息。
- 不需要登录，不收集遥测，不创建后台事件日志。
- 捕获的 Ghost 事件只保存在内存中，退出程序后消失。
- 默认以普通用户权限运行，不会自动请求管理员权限。
- 对管理员进程和受保护进程，Windows 可能拒绝读取路径、命令行、UI 属性或拒绝结束进程；Peek 会显示系统允许提供的信息。

## 从源代码构建

需要：

- Visual Studio 2022 Build Tools，并安装 **Desktop development with C++** 工作负载
- CMake 3.24 或更高版本

在 PowerShell 中运行：

```powershell
./scripts/build-release.ps1
```

使用 Visual Studio 多配置生成器时，输出文件位于：

```text
build/release/bin/Release/Peek.exe
```

自动与手动验收步骤见 [TESTING.md](TESTING.md)，架构和 Windows API 选择见 [ARCHITECTURE.md](ARCHITECTURE.md)。

## 当前版本范围

- Windows 10 / 11 x64
- 一个原生 Win32 窗口和一个托盘图标
- 最近 100 条 Ghost 内存事件
- Unlock 文件选择与拖放、定位程序、确认后结束进程
- Focus 的 Win32 与 UI Automation 属性、高亮和 Esc 退出
- 简体中文 / English 即时切换
- 关闭按钮行为设置

当前没有自动更新、网络功能、账户、插件系统、数据库、完整任务管理器、强制关闭 Handle 或大型日志系统。

<div align="right"><a href="#top">返回顶部</a></div>

---

<a id="readme-en"></a>

## English

Peek is a small, fast Windows observation utility. It is not a task manager and does not try to replace PowerToys. It focuses on three questions that Windows often cannot answer immediately.

| Feature | Question answered | When it helps |
| --- | --- | --- |
| **Ghost** | What was that window or process that flashed past? | A console, script, updater, or installer appeared and vanished |
| **Unlock** | Which process is using this file? | A file cannot be deleted, moved, renamed, or replaced |
| **Focus** | What Windows UI element is under the pointer? | You need to identify a window, button, field, process, or automation control |

### Why Peek

- **Small and direct:** all three tools share one compact `420 × 540` window.
- **Truly native:** C++20, Win32, and Windows system components; no Electron, WebView, Qt, or .NET Runtime.
- **Offline by design:** no account, telemetry, cloud service, online database, or network dependency.
- **Always available:** a tray icon and global hotkeys keep the tools one action away.
- **Bilingual:** follows the Windows display language on first launch and remembers later changes.

## Download and start

The prebuilt Windows x64 application is available at [dist/Peek.exe](dist/Peek.exe).

1. Download `Peek.exe`.
2. Run it directly; no installation is required.
3. Peek opens its main window and adds an icon to the notification area.
4. The current build is not commercially code-signed. If SmartScreen reports an unknown publisher, first confirm that the file came from this repository.

> Requirements: Windows 10 or Windows 11, x64.

## Features in detail

### Ghost

> Answers: “What exactly flashed past just now?”

Ghost observes short-lived processes and top-level window events. After a console, script host, updater, or installer appears and vanishes, press `Ctrl + Shift + G` to see the latest event immediately.

Records include as much of the following as Windows makes available:

- Time, executable name, and PID
- Process start/exit and window create/show/hide/destroy events
- Process lifetime or window lifetime
- Executable path and command line
- Parent process and parent executable path
- Window title

Select a card to expand its details. Ghost keeps only the latest 100 events in memory and writes no background log. Paths and command lines are best-effort for processes that exit very quickly. If local policy blocks WMI subscriptions, Peek automatically falls back to a low-cost 250 ms process snapshot while window events remain event-driven.

### Unlock

> Answers: “Who is using this file?”

Open **Unlock**, drop a file onto the window or use the file picker, and Peek lists the applications that Windows reports as using it. Each result can show the process name, PID, and executable path. You can reveal the executable or terminate the process after confirmation.

Unlock uses the official Windows Restart Manager API. It intentionally does not force-close arbitrary handles, which could corrupt data or destabilize an application. Kernel drivers, memory mappings, and owners that Restart Manager cannot identify may not appear.

> Terminating a process can discard unsaved work. Try closing the application normally first.

### Focus

> Answers: “What Windows UI is under the pointer?”

Press `Ctrl + Shift + F` or choose **Start Inspect**, then move the pointer over a window or control. Peek highlights the target and reports available Win32 and UI Automation properties in real time. Press `Esc` to stop.

Available fields include:

- HWND, process name, PID, and executable path
- Window Class and Window Title
- Position, width, and height
- AutomationId, ControlType, Name, and Value
- UI Automation BoundingRectangle

Elevated, protected, secure-desktop, or custom-rendered applications may expose only part of this information.

## Hotkeys and tray

| Action | Shortcut or command |
| --- | --- |
| Show the latest Ghost events | `Ctrl + Shift + G` |
| Start Focus inspection | `Ctrl + Shift + F` |
| Stop Focus inspection | `Esc` |
| Reopen the main window | Single- or double-click the tray icon |
| Open the tray menu | Right-click the tray icon |
| Fully stop Peek | Tray menu → **Exit** |

The tray menu provides **Open Peek**, **Settings**, **Language**, and **Exit**.

## Language switching

Peek includes Simplified Chinese and English without extra language packs.

- Click `中文 / EN` in the top-right corner of the main window.
- Or right-click the tray icon and choose a language from the **Language** submenu.
- On first launch, Peek follows the Windows display language.
- After a manual change, Peek remembers the selection for later launches.

Use [简体中文](#readme-zh) / [English](#readme-en) at the top of this README to switch the documentation language.

## Closing and settings

The first time you click `X`, Peek asks whether it should:

- **Minimize to tray:** hide the window while Ghost capture and hotkeys remain active.
- **Exit Peek:** stop the application, listeners, and global hotkeys completely.

Enable **Remember my choice** to apply the selection automatically next time. You can reset it from tray **Settings** to **Ask every time**, **Minimize to tray**, or **Exit Peek**. Language and close behavior are stored only for the current Windows user in the local registry.

## Privacy and permissions

- No network access, accounts, telemetry, cloud sync, or event database.
- Captured Ghost events remain in memory and disappear when Peek exits.
- Peek normally runs unelevated and does not request elevation automatically.
- Windows may deny paths, command lines, UI properties, or process termination for elevated and protected processes. Peek displays whatever the operating system permits.

## Build from source

Requirements:

- Visual Studio 2022 Build Tools with **Desktop development with C++**
- CMake 3.24 or newer

Run from PowerShell:

```powershell
./scripts/build-release.ps1
```

With the Visual Studio multi-config generator, the executable is written to:

```text
build/release/bin/Release/Peek.exe
```

See [TESTING.md](TESTING.md) for acceptance testing and [ARCHITECTURE.md](ARCHITECTURE.md) for architecture and Windows API rationale.

## Current release scope

- Windows 10 / 11 x64
- One native Win32 window and one tray icon
- The latest 100 Ghost events, held in memory
- Unlock file picker and drag-and-drop, reveal executable, confirmed process termination
- Focus Win32/UI Automation details, target highlight, and `Esc` to exit
- Instant Simplified Chinese / English switching
- Close-button behavior setting

There is currently no updater, networking, account system, plug-in system, database, full task manager, force-close-handle feature, or large logging subsystem.

<div align="right"><a href="#top">Back to top</a></div>
