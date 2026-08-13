#include "core/process_resolver.h"

#include <tlhelp32.h>
#include <wbemidl.h>
#include <wrl/client.h>

#include <filesystem>
#include <vector>

namespace peek {
namespace {

using Microsoft::WRL::ComPtr;

std::wstring VariantString(const VARIANT& value) {
    if (value.vt == VT_BSTR && value.bstrVal) return value.bstrVal;
    return {};
}

std::wstring QueryWmiProperty(DWORD pid, const wchar_t* property) {
    ComPtr<IWbemLocator> locator;
    if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&locator)))) return {};

    ComPtr<IWbemServices> services;
    BSTR ns = SysAllocString(L"ROOT\\CIMV2");
    const HRESULT connect = locator->ConnectServer(ns, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
    SysFreeString(ns);
    if (FAILED(connect)) return {};
    CoSetProxyBlanket(services.Get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                      RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

    const std::wstring query_text = L"SELECT " + std::wstring(property) + L" FROM Win32_Process WHERE ProcessId=" + std::to_wstring(pid);
    BSTR language = SysAllocString(L"WQL");
    BSTR query = SysAllocString(query_text.c_str());
    ComPtr<IEnumWbemClassObject> enumerator;
    const HRESULT exec = services->ExecQuery(language, query,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
    SysFreeString(language);
    SysFreeString(query);
    if (FAILED(exec)) return {};

    ComPtr<IWbemClassObject> object;
    ULONG returned = 0;
    if (FAILED(enumerator->Next(200, 1, &object, &returned)) || returned == 0) return {};
    VARIANT value;
    VariantInit(&value);
    const HRESULT get = object->Get(property, 0, &value, nullptr, nullptr);
    std::wstring result = SUCCEEDED(get) ? VariantString(value) : std::wstring{};
    VariantClear(&value);
    return result;
}

} // namespace

std::wstring ProcessResolver::FileName(const std::wstring& path) {
    if (path.empty()) return {};
    return std::filesystem::path(path).filename().wstring();
}

std::wstring ProcessResolver::QueryImagePath(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return {};
    std::vector<wchar_t> buffer(32768);
    DWORD size = static_cast<DWORD>(buffer.size());
    const bool ok = QueryFullProcessImageNameW(process, 0, buffer.data(), &size) != FALSE;
    CloseHandle(process);
    return ok ? std::wstring(buffer.data(), size) : std::wstring{};
}

std::wstring ProcessResolver::QueryCommandLine(DWORD pid) {
    return QueryWmiProperty(pid, L"CommandLine");
}

DWORD ProcessResolver::QueryParentPid(DWORD pid) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    DWORD parent = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (entry.th32ProcessID == pid) {
                parent = entry.th32ParentProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return parent;
}

bool ProcessResolver::QueryStartTime(DWORD pid, FILETIME& start_time) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    FILETIME exit{}, kernel{}, user{};
    const bool ok = GetProcessTimes(process, &start_time, &exit, &kernel, &user) != FALSE;
    CloseHandle(process);
    return ok;
}

ProcessInfo ProcessResolver::Resolve(DWORD pid, DWORD known_parent, const std::wstring& known_name,
                                     bool include_command_line) const {
    ProcessInfo info;
    info.pid = pid;
    info.parent_pid = known_parent ? known_parent : QueryParentPid(pid);
    info.exe_path = QueryImagePath(pid);
    info.name = !known_name.empty() ? known_name : FileName(info.exe_path);
    if (include_command_line) info.command_line = QueryCommandLine(pid);
    info.accessible = !info.exe_path.empty();
    (void)QueryStartTime(pid, info.start_time);

    if (info.parent_pid) {
        info.parent_path = QueryImagePath(info.parent_pid);
        info.parent_name = FileName(info.parent_path);
        if (info.parent_name.empty()) info.parent_name = L"PID " + std::to_wstring(info.parent_pid);
    }
    return info;
}

} // namespace peek
