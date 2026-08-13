#include "self_test.h"

#include "core/event_store.h"
#include "core/process_resolver.h"
#include "focus/focus_inspector.h"
#include "ghost/ghost_monitor.h"
#include "unlock/unlock_service.h"

#include <objbase.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

namespace peek {
namespace {

struct Check {
    std::wstring name;
    bool passed{};
    std::wstring detail;
};

void WriteUtf8(const std::wstring& path, const std::wstring& text) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), utf8.data(), size, nullptr, nullptr);
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    const BYTE bom[] = {0xEF, 0xBB, 0xBF}; DWORD written{};
    WriteFile(file, bom, sizeof(bom), &written, nullptr);
    WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    CloseHandle(file);
}

std::wstring DefaultReportPath() {
    wchar_t temp[MAX_PATH]{}; GetTempPathW(static_cast<DWORD>(std::size(temp)), temp);
    return std::wstring(temp) + L"Peek-selftest.txt";
}

} // namespace

int RunSelfTest(const std::wstring& requested_report_path) {
    const std::wstring report_path = requested_report_path.empty() ? DefaultReportPath() : requested_report_path;
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
                         RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

    std::vector<Check> checks;
    ProcessResolver resolver;
    EventStore store(100);

    // Bounded-store and lifetime correlation test.
    GhostEvent start{}; start.kind = GhostKind::ProcessStarted; start.timestamp = std::chrono::system_clock::now();
    start.process.pid = 4242; start.process.exe_path = L"C:\\test\\short.exe"; start.process.command_line = L"short.exe --test";
    store.Add(start);
    GhostEvent stop{}; stop.kind = GhostKind::ProcessExited; stop.timestamp = start.timestamp + std::chrono::milliseconds(137); stop.process.pid = 4242;
    store.Add(stop);
    const auto correlated = store.Latest();
    checks.push_back({L"EventStore correlation", correlated && correlated->has_lifetime && correlated->lifetime.count() == 137 &&
        correlated->process.command_line == start.process.command_line, L"Expected a 137 ms merged lifecycle."});
    store.Clear();

    // Real WMI process trace test with a deliberately short child process.
    GhostMonitor ghost(store, resolver, {});
    std::wstring ghost_error;
    bool ghost_ok = ghost.Start(ghost_error);
    DWORD short_pid = 0;
    if (ghost_ok) {
        wchar_t command[] = L"cmd.exe /d /c exit 0";
        STARTUPINFOW startup{}; startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (CreateProcessW(nullptr, command, nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &startup, &process)) {
            short_pid = process.dwProcessId;
            Sleep(400);
            ResumeThread(process.hThread);
            WaitForSingleObject(process.hProcess, 3000);
            CloseHandle(process.hThread); CloseHandle(process.hProcess);
            const ULONGLONG deadline = GetTickCount64() + 5000;
            while (GetTickCount64() < deadline) {
                const auto events = store.Snapshot();
                bool started = false, stopped = false;
                for (const auto& item : events) if (item.process.pid == short_pid) {
                    started |= item.kind == GhostKind::ProcessStarted;
                    stopped |= item.kind == GhostKind::ProcessExited;
                }
                if (started && stopped) break;
                Sleep(100);
            }
        } else {
            ghost_ok = false; ghost_error = L"CreateProcess failed: " + std::to_wstring(GetLastError());
        }
    }
    bool saw_start = false, saw_stop = false;
    for (const auto& item : store.Snapshot()) if (item.process.pid == short_pid) {
        saw_start |= item.kind == GhostKind::ProcessStarted;
        saw_stop |= item.kind == GhostKind::ProcessExited;
    }
    checks.push_back({L"Ghost short process", ghost_ok && saw_start && saw_stop,
                      ghost_error.empty() ? L"Expected start and stop events for cmd.exe." : ghost_error});
    ghost.Stop();

    // Real Restart Manager test: this process holds an exclusive temporary file lock.
    wchar_t temp_dir[MAX_PATH]{}, temp_file[MAX_PATH]{};
    GetTempPathW(static_cast<DWORD>(std::size(temp_dir)), temp_dir);
    GetTempFileNameW(temp_dir, L"PEK", 0, temp_file);
    HANDLE locked = CreateFileW(temp_file, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    UnlockService unlock(resolver);
    std::wstring unlock_error;
    const auto owners = unlock.Query(temp_file, unlock_error);
    bool found_self = false;
    for (const auto& owner : owners) found_self |= owner.process.pid == GetCurrentProcessId();
    checks.push_back({L"Unlock exclusive file", locked != INVALID_HANDLE_VALUE && unlock_error.empty() && found_self,
                      unlock_error.empty() ? L"Expected Restart Manager to report the self-test process." : unlock_error});
    if (locked != INVALID_HANDLE_VALUE) CloseHandle(locked);
    DeleteFileW(temp_file);

    // Focus test against a native Win32 button, including UI Automation enrichment.
    HWND host = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"Peek Focus test", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                80, 80, 320, 180, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    HWND button = host ? CreateWindowExW(0, L"BUTTON", L"Inspect me", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                          60, 55, 160, 40, host, nullptr, GetModuleHandleW(nullptr), nullptr) : nullptr;
    if (host) {
        SetWindowPos(host, HWND_TOPMOST, 80, 80, 320, 180, SWP_SHOWWINDOW);
        SetForegroundWindow(host);
    }
    UpdateWindow(host);
    FocusInspector inspector(resolver, {});
    FocusSnapshot focus_snapshot;
    std::wstring focus_error;
    bool focus_received = false;
    bool uia_received = false;
    if (button) {
        RECT rect{}; GetWindowRect(button, &rect);
        POINT point{(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2};
        std::atomic_bool inspection_done{false};
        bool sampled = false;
        std::thread inspection([&] {
            sampled = inspector.InspectOnce(point, focus_snapshot, focus_error);
            inspection_done = true;
        });
        const ULONGLONG deadline = GetTickCount64() + 5000;
        while (!inspection_done && GetTickCount64() < deadline) {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            Sleep(10);
        }
        inspection.join();
        focus_received = sampled && focus_snapshot.process.pid == GetCurrentProcessId();
        uia_received = focus_snapshot.automation.available && !focus_snapshot.automation.control_type.empty();
    }
    std::wstring focus_detail = focus_error;
    if (focus_detail.empty() && !(button && focus_received && uia_received)) {
        focus_detail = L"Hit PID " + std::to_wstring(focus_snapshot.process.pid) +
            L", class '" + focus_snapshot.window.class_name + L"', UIA available " +
            (focus_snapshot.automation.available ? L"yes" : L"no") + L", control type '" +
            focus_snapshot.automation.control_type + L"'.";
    }
    checks.push_back({L"Focus Win32/UIA", button && focus_received && uia_received,
                      focus_detail.empty() ? L"Expected HWND/process data and a UI Automation control type for a native button."
                                           : focus_detail});
    if (host) DestroyWindow(host);

    bool all_passed = true;
    std::wstring report = L"Peek self-test\r\n==============\r\n";
    for (const auto& check : checks) {
        all_passed &= check.passed;
        report += check.passed ? L"PASS  " : L"FAIL  ";
        report += check.name + L"\r\n      " + check.detail + L"\r\n";
    }
    report += L"\r\nResult: " + std::wstring(all_passed ? L"PASS" : L"FAIL") + L"\r\n";
    WriteUtf8(report_path, report);
    if (SUCCEEDED(com)) CoUninitialize();
    return all_passed ? 0 : 2;
}

} // namespace peek
