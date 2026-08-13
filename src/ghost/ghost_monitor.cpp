#include "ghost/ghost_monitor.h"

#include <psapi.h>
#include <wbemidl.h>
#include <wrl/client.h>

#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace peek {

using Microsoft::WRL::ComPtr;
GhostMonitor* GhostMonitor::instance_ = nullptr;

namespace {

DWORD WmiDword(IWbemClassObject* object, const wchar_t* name) {
    VARIANT value;
    VariantInit(&value);
    DWORD result = 0;
    if (SUCCEEDED(object->Get(name, 0, &value, nullptr, nullptr))) {
        if (value.vt == VT_UI4 || value.vt == VT_UINT) result = value.ulVal;
        else if (value.vt == VT_I4 || value.vt == VT_INT) result = static_cast<DWORD>(value.lVal);
    }
    VariantClear(&value);
    return result;
}

std::wstring WmiString(IWbemClassObject* object, const wchar_t* name) {
    VARIANT value;
    VariantInit(&value);
    std::wstring result;
    if (SUCCEEDED(object->Get(name, 0, &value, nullptr, nullptr)) && value.vt == VT_BSTR && value.bstrVal) {
        result = value.bstrVal;
    }
    VariantClear(&value);
    return result;
}

WindowInfo SnapshotWindow(HWND hwnd) {
    WindowInfo info;
    info.hwnd = hwnd;
    GetWindowThreadProcessId(hwnd, &info.pid);
    wchar_t text[512]{};
    const int title_len = GetWindowTextW(hwnd, text, static_cast<int>(std::size(text)));
    if (title_len > 0) info.title.assign(text, title_len);
    const int class_len = GetClassNameW(hwnd, text, static_cast<int>(std::size(text)));
    if (class_len > 0) info.class_name.assign(text, class_len);
    GetWindowRect(hwnd, &info.bounds);
    return info;
}

std::wstring HResultText(const wchar_t* operation, HRESULT result) {
    std::wstringstream text;
    text << operation << L" failed (0x" << std::uppercase << std::hex
         << static_cast<unsigned long>(result) << L").";
    return text.str();
}

std::unordered_map<DWORD, ProcessInfo> SnapshotProcesses() {
    std::unordered_map<DWORD, ProcessInfo> processes;
    std::vector<DWORD> pids(1024);
    DWORD bytes = 0;
    while (EnumProcesses(pids.data(), static_cast<DWORD>(pids.size() * sizeof(DWORD)), &bytes)) {
        if (bytes < pids.size() * sizeof(DWORD)) break;
        pids.resize(pids.size() * 2);
    }
    const std::size_t count = bytes / sizeof(DWORD);
    processes.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        ProcessInfo process;
        process.pid = pids[index];
        if (process.pid) processes.emplace(process.pid, std::move(process));
    }
    return processes;
}

} // namespace

GhostMonitor::GhostMonitor(EventStore& store, ProcessResolver& resolver, Notify notify)
    : store_(store), resolver_(resolver), notify_(std::move(notify)) {}

GhostMonitor::~GhostMonitor() { Stop(); }

bool GhostMonitor::Start(std::wstring& error) {
    error.clear();
    if (instance_) {
        error = L"Only one Ghost monitor can run in a process.";
        return false;
    }
    instance_ = this;
    stopping_ = false;
    wmi_available_ = false;
    wmi_error_.clear();
    stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    wmi_ready_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    poll_ready_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stop_event_ || !wmi_ready_ || !poll_ready_) {
        error = L"Ghost synchronization events could not be created.";
        if (stop_event_) { CloseHandle(stop_event_); stop_event_ = nullptr; }
        if (wmi_ready_) { CloseHandle(wmi_ready_); wmi_ready_ = nullptr; }
        if (poll_ready_) { CloseHandle(poll_ready_); poll_ready_ = nullptr; }
        instance_ = nullptr;
        return false;
    }
    create_destroy_hook_ = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY, nullptr,
        WinEventCallback, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    show_hide_hook_ = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_HIDE, nullptr,
        WinEventCallback, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    wmi_thread_ = std::thread([this] { WmiLoop(); });
    if (WaitForSingleObject(wmi_ready_, 3000) != WAIT_OBJECT_0 || !wmi_available_) {
        {
            std::lock_guard lock(status_mutex_);
            error = wmi_error_.empty() ? L"WMI process trace could not be started." : wmi_error_;
        }
        error += L" Using the 250 ms process snapshot fallback; command lines may be unavailable.";
        poll_thread_ = std::thread([this] { PollLoop(); });
        if (WaitForSingleObject(poll_ready_, 3000) != WAIT_OBJECT_0) {
            error = L"The process snapshot fallback did not become ready.";
            Stop();
            return false;
        }
    }
    if (!create_destroy_hook_ && !show_hide_hook_ && !wmi_available_ && !poll_thread_.joinable()) {
        error = L"Neither process nor window monitoring could be started.";
        Stop();
        return false;
    }
    return true;
}

void GhostMonitor::Stop() {
    stopping_ = true;
    if (stop_event_) SetEvent(stop_event_);
    if (create_destroy_hook_) { UnhookWinEvent(create_destroy_hook_); create_destroy_hook_ = nullptr; }
    if (show_hide_hook_) { UnhookWinEvent(show_hide_hook_); show_hide_hook_ = nullptr; }
    if (wmi_thread_.joinable()) wmi_thread_.join();
    if (poll_thread_.joinable()) poll_thread_.join();
    if (wmi_ready_) { CloseHandle(wmi_ready_); wmi_ready_ = nullptr; }
    if (poll_ready_) { CloseHandle(poll_ready_); poll_ready_ = nullptr; }
    if (stop_event_) { CloseHandle(stop_event_); stop_event_ = nullptr; }
    if (instance_ == this) instance_ = nullptr;
}

void CALLBACK GhostMonitor::WinEventCallback(HWINEVENTHOOK, DWORD event, HWND hwnd,
    LONG object_id, LONG child_id, DWORD, DWORD) {
    if (!instance_ || !hwnd || object_id != OBJID_WINDOW || child_id != CHILDID_SELF) return;
    instance_->OnWindowEvent(event, hwnd);
}

void GhostMonitor::OnWindowEvent(DWORD event, HWND hwnd) {
    if (GetAncestor(hwnd, GA_ROOT) != hwnd) return;
    const auto now = std::chrono::system_clock::now();
    GhostEvent item;
    item.timestamp = now;

    {
        std::lock_guard lock(window_mutex_);
        if (event == EVENT_OBJECT_CREATE || event == EVENT_OBJECT_SHOW) {
            item.window = SnapshotWindow(hwnd);
            window_cache_[hwnd] = item.window;
            window_starts_.try_emplace(hwnd, now);
        } else {
            if (auto found = window_cache_.find(hwnd); found != window_cache_.end()) item.window = found->second;
            else item.window = SnapshotWindow(hwnd);
            if (event == EVENT_OBJECT_DESTROY) {
                if (auto start = window_starts_.find(hwnd); start != window_starts_.end()) {
                    item.lifetime = std::chrono::duration_cast<std::chrono::milliseconds>(now - start->second);
                    item.has_lifetime = true;
                    window_starts_.erase(start);
                }
                window_cache_.erase(hwnd);
            }
        }
    }

    if (!item.window.pid) return;
    item.process = resolver_.Resolve(item.window.pid);
    switch (event) {
        case EVENT_OBJECT_CREATE: item.kind = GhostKind::WindowCreated; break;
        case EVENT_OBJECT_SHOW: item.kind = GhostKind::WindowShown; break;
        case EVENT_OBJECT_HIDE: item.kind = GhostKind::WindowHidden; break;
        case EVENT_OBJECT_DESTROY: item.kind = GhostKind::WindowDestroyed; break;
        default: return;
    }
    store_.Add(std::move(item));
    if (notify_) notify_();
}

void GhostMonitor::WmiLoop() {
    const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    auto fail = [&](std::wstring message) {
        std::lock_guard lock(status_mutex_);
        wmi_error_ = std::move(message);
    };
    if (FAILED(init)) {
        fail(HResultText(L"CoInitializeEx", init));
        if (wmi_ready_) SetEvent(wmi_ready_);
        return;
    }

    ComPtr<IWbemLocator> locator;
    ComPtr<IWbemServices> services;
    ComPtr<IEnumWbemClassObject> start_events;
    ComPtr<IEnumWbemClassObject> stop_events;
    HRESULT status = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&locator));
    if (SUCCEEDED(status)) {
        BSTR ns = SysAllocString(L"ROOT\\CIMV2");
        const HRESULT connected = locator->ConnectServer(ns, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
        SysFreeString(ns);
        if (SUCCEEDED(connected)) {
            status = CoSetProxyBlanket(services.Get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                                       RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
            auto subscribe = [&](const wchar_t* query_text, ComPtr<IEnumWbemClassObject>& target) {
                if (FAILED(status)) return status;
                BSTR language = SysAllocString(L"WQL");
                BSTR query = SysAllocString(query_text);
                const HRESULT subscribed = services->ExecNotificationQuery(language, query,
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &target);
                SysFreeString(language);
                SysFreeString(query);
                return subscribed;
            };
            status = subscribe(L"SELECT * FROM Win32_ProcessStartTrace", start_events);
            if (SUCCEEDED(status)) status = subscribe(L"SELECT * FROM Win32_ProcessStopTrace", stop_events);
        } else status = connected;
    }

    wmi_available_ = start_events != nullptr && stop_events != nullptr;
    if (!wmi_available_) fail(HResultText(L"WMI process subscription", status));
    if (wmi_ready_) SetEvent(wmi_ready_);

    auto consume = [&](IEnumWbemClassObject* enumerator, GhostKind kind) {
        IWbemClassObject* objects[16]{};
        ULONG returned = 0;
        const HRESULT next = enumerator->Next(100, static_cast<ULONG>(std::size(objects)), objects, &returned);
        if (FAILED(next) && next != WBEM_S_TIMEDOUT) return false;
        for (ULONG i = 0; i < returned; ++i) {
            const DWORD pid = WmiDword(objects[i], L"ProcessID");
            const DWORD parent = WmiDword(objects[i], L"ParentProcessID");
            const std::wstring name = WmiString(objects[i], L"ProcessName");
            if (pid) {
                GhostEvent item;
                item.timestamp = std::chrono::system_clock::now();
                item.kind = kind;
                item.process = resolver_.Resolve(pid, parent, name, kind == GhostKind::ProcessStarted);
                if (item.process.name.empty()) item.process.name = name;
                if (!item.process.parent_pid) item.process.parent_pid = parent;
                store_.Add(std::move(item));
                if (notify_) notify_();
            }
            objects[i]->Release();
        }
        return true;
    };

    while (!stopping_ && start_events && stop_events) {
        if (!consume(start_events.Get(), GhostKind::ProcessStarted)) break;
        if (!consume(stop_events.Get(), GhostKind::ProcessExited)) break;
    }

    stop_events.Reset();
    start_events.Reset();
    services.Reset();
    locator.Reset();
    CoFreeUnusedLibrariesEx(0, 0);
    CoUninitialize();
}

void GhostMonitor::PollLoop() {
    auto known = SnapshotProcesses();
    std::unordered_set<DWORD> observed_starts;
    if (poll_ready_) SetEvent(poll_ready_);
    while (!stopping_) {
        if (WaitForSingleObject(stop_event_, 250) != WAIT_TIMEOUT) break;
        auto current = SnapshotProcesses();

        for (auto& [pid, process] : current) {
            if (auto previous = known.find(pid); previous != known.end()) {
                process = previous->second;
                continue;
            }
            // Keep the capture loop fast. QueryCommandLine uses WMI and can stall long
            // enough to miss the next short-lived process; WMI-backed monitoring still
            // performs that enrichment when it is available.
            ProcessInfo resolved = resolver_.Resolve(pid, process.parent_pid, process.name, false);
            if (resolved.name.empty()) resolved.name = process.name;
            if (resolved.name.empty() && resolved.exe_path.empty()) continue;
            GhostEvent event;
            event.timestamp = std::chrono::system_clock::now();
            event.kind = GhostKind::ProcessStarted;
            event.process = resolved;
            store_.Add(std::move(event));
            if (notify_) notify_();
            process = std::move(resolved);
            observed_starts.insert(pid);
        }

        for (const auto& [pid, process] : known) {
            if (current.contains(pid)) continue;
            if (!observed_starts.erase(pid)) continue;
            GhostEvent event;
            event.timestamp = std::chrono::system_clock::now();
            event.kind = GhostKind::ProcessExited;
            event.process = process;
            store_.Add(std::move(event));
            if (notify_) notify_();
        }
        known = std::move(current);
    }
}

} // namespace peek
