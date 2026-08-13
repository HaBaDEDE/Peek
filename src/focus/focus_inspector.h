#pragma once

#include "core/process_resolver.h"
#include "core/types.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

namespace peek {

class FocusInspector {
public:
    using SnapshotCallback = std::function<void(FocusSnapshot)>;
    FocusInspector(ProcessResolver& resolver, SnapshotCallback callback);
    ~FocusInspector();
    void Start();
    void Stop();
    [[nodiscard]] bool InspectOnce(POINT point, FocusSnapshot& snapshot, std::wstring& error) const;
    [[nodiscard]] bool Active() const { return active_; }

private:
    void Loop();
    ProcessResolver& resolver_;
    SnapshotCallback callback_;
    std::atomic_bool active_{false};
    std::thread worker_;
};

} // namespace peek
