#pragma once

#include "core/event_store.h"
#include "core/process_resolver.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace peek {

class GhostMonitor {
public:
    using Notify = std::function<void()>;

    GhostMonitor(EventStore& store, ProcessResolver& resolver, Notify notify);
    ~GhostMonitor();
    bool Start(std::wstring& error);
    void Stop();

private:
    static void CALLBACK WinEventCallback(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
        LONG object_id, LONG child_id, DWORD event_thread, DWORD event_time);
    void OnWindowEvent(DWORD event, HWND hwnd);
    void WmiLoop();
    void PollLoop();

    EventStore& store_;
    ProcessResolver& resolver_;
    Notify notify_;
    std::atomic_bool stopping_{false};
    std::atomic_bool wmi_available_{false};
    std::thread wmi_thread_;
    std::thread poll_thread_;
    HANDLE wmi_ready_{};
    HANDLE stop_event_{};
    HANDLE poll_ready_{};
    std::mutex status_mutex_;
    std::wstring wmi_error_;
    HWINEVENTHOOK create_destroy_hook_{};
    HWINEVENTHOOK show_hide_hook_{};
    std::mutex window_mutex_;
    std::unordered_map<HWND, std::chrono::system_clock::time_point> window_starts_;
    std::unordered_map<HWND, WindowInfo> window_cache_;
    static GhostMonitor* instance_;
};

} // namespace peek
