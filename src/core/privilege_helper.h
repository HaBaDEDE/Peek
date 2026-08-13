#pragma once

#include <windows.h>
#include <string>

namespace peek {

class PrivilegeHelper {
public:
    [[nodiscard]] static bool IsCurrentProcessElevated();
    [[nodiscard]] static bool Terminate(DWORD pid, const FILETIME* expected_start_time, std::wstring& error);
};

} // namespace peek
