#include "core/privilege_helper.h"

#include <vector>

namespace peek {
namespace {
std::wstring Win32Error(DWORD code) {
    wchar_t* text = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<wchar_t*>(&text), 0, nullptr);
    std::wstring result = text ? text : L"Unknown error";
    if (text) LocalFree(text);
    return result;
}
}

bool PrivilegeHelper::IsCurrentProcessElevated() {
    HANDLE token{};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size{};
    const bool ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) != FALSE;
    CloseHandle(token);
    return ok && elevation.TokenIsElevated != 0;
}

bool PrivilegeHelper::Terminate(DWORD pid, const FILETIME* expected_start_time, std::wstring& error) {
    if (pid == 0 || pid == GetCurrentProcessId()) {
        error = L"Refusing to terminate this process.";
        return false;
    }
    HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
    if (!process) {
        error = Win32Error(GetLastError());
        return false;
    }
    if (expected_start_time && (expected_start_time->dwLowDateTime || expected_start_time->dwHighDateTime)) {
        FILETIME current_start{}, exit{}, kernel{}, user{};
        if (!GetProcessTimes(process, &current_start, &exit, &kernel, &user) ||
            CompareFileTime(&current_start, expected_start_time) != 0) {
            CloseHandle(process);
            error = L"The original process has already exited; its PID may have been reused.";
            return false;
        }
    }
    const bool ok = TerminateProcess(process, 1) != FALSE;
    if (!ok) error = Win32Error(GetLastError());
    CloseHandle(process);
    return ok;
}

} // namespace peek
