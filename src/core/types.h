#pragma once

#include <windows.h>
#include <chrono>
#include <cstdint>
#include <string>

namespace peek {

struct ProcessInfo {
    DWORD pid{};
    DWORD parent_pid{};
    std::wstring name;
    std::wstring exe_path;
    std::wstring command_line;
    std::wstring parent_name;
    std::wstring parent_path;
    FILETIME start_time{};
    bool elevated{};
    bool accessible{};
};

struct WindowInfo {
    HWND hwnd{};
    DWORD pid{};
    std::wstring class_name;
    std::wstring title;
    RECT bounds{};
};

struct AutomationInfo {
    bool available{};
    std::wstring automation_id;
    std::wstring control_type;
    std::wstring name;
    std::wstring value;
    RECT bounds{};
};

enum class GhostKind {
    ProcessStarted,
    ProcessExited,
    WindowCreated,
    WindowShown,
    WindowHidden,
    WindowDestroyed,
};

struct GhostEvent {
    std::uint64_t sequence{};
    GhostKind kind{};
    std::chrono::system_clock::time_point timestamp{};
    ProcessInfo process;
    WindowInfo window;
    std::chrono::milliseconds lifetime{};
    bool has_lifetime{};
};

struct LockOwner {
    ProcessInfo process;
    std::wstring application_name;
    std::wstring service_name;
    DWORD app_status{};
    bool restartable{};
};

struct FocusSnapshot {
    WindowInfo window;
    ProcessInfo process;
    AutomationInfo automation;
    POINT cursor{};
};

} // namespace peek
