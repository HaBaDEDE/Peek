#pragma once

#include "core/types.h"

#include <string>

namespace peek {

class ProcessResolver {
public:
    [[nodiscard]] ProcessInfo Resolve(DWORD pid, DWORD known_parent = 0,
                                      const std::wstring& known_name = {}, bool include_command_line = false) const;
    [[nodiscard]] static std::wstring FileName(const std::wstring& path);
    [[nodiscard]] static std::wstring QueryImagePath(DWORD pid);
    [[nodiscard]] static std::wstring QueryCommandLine(DWORD pid);
    [[nodiscard]] static DWORD QueryParentPid(DWORD pid);
    [[nodiscard]] static bool QueryStartTime(DWORD pid, FILETIME& start_time);
};

} // namespace peek
