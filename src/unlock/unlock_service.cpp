#include "unlock/unlock_service.h"

#include <restartmanager.h>

#include <array>
#include <vector>

namespace peek {

std::vector<LockOwner> UnlockService::Query(const std::wstring& file_path, std::wstring& error) const {
    std::vector<LockOwner> result;
    if (file_path.empty()) { error = L"Choose or drop a file first."; return result; }

    DWORD session = 0;
    std::array<wchar_t, CCH_RM_SESSION_KEY + 1> key{};
    DWORD status = RmStartSession(&session, 0, key.data());
    if (status != ERROR_SUCCESS) { error = L"Restart Manager session failed: " + std::to_wstring(status); return result; }

    const wchar_t* files[] = {file_path.c_str()};
    status = RmRegisterResources(session, 1, files, 0, nullptr, 0, nullptr);
    if (status != ERROR_SUCCESS) {
        error = L"The file could not be registered: " + std::to_wstring(status);
        RmEndSession(session);
        return result;
    }

    UINT needed = 0;
    UINT count = 0;
    DWORD reboot_reasons = 0;
    status = RmGetList(session, &needed, &count, nullptr, &reboot_reasons);
    if (status == ERROR_SUCCESS && needed == 0) {
        RmEndSession(session);
        return result;
    }
    if (status != ERROR_MORE_DATA) {
        error = L"Lock query failed: " + std::to_wstring(status);
        RmEndSession(session);
        return result;
    }

    std::vector<RM_PROCESS_INFO> owners(needed);
    count = needed;
    status = RmGetList(session, &needed, &count, owners.data(), &reboot_reasons);
    RmEndSession(session);
    if (status != ERROR_SUCCESS) { error = L"Lock query failed: " + std::to_wstring(status); return result; }

    for (UINT i = 0; i < count; ++i) {
        LockOwner owner;
        owner.process = resolver_.Resolve(owners[i].Process.dwProcessId);
        owner.process.start_time = owners[i].Process.ProcessStartTime;
        owner.application_name = owners[i].strAppName;
        owner.service_name = owners[i].strServiceShortName;
        owner.app_status = owners[i].AppStatus;
        owner.restartable = owners[i].bRestartable != FALSE;
        if (owner.process.name.empty()) owner.process.name = owner.application_name;
        result.push_back(std::move(owner));
    }
    return result;
}

} // namespace peek
