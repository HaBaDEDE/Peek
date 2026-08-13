#pragma once

#include "core/types.h"

#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace peek {

class EventStore {
public:
    explicit EventStore(std::size_t capacity = 100);

    void Add(GhostEvent event);
    [[nodiscard]] std::vector<GhostEvent> Snapshot() const;
    [[nodiscard]] std::optional<GhostEvent> Latest() const;
    void Clear();

private:
    struct StoredStart {
        std::chrono::system_clock::time_point timestamp;
        ProcessInfo process;
    };

    std::size_t capacity_;
    mutable std::mutex mutex_;
    std::deque<GhostEvent> events_;
    std::unordered_map<DWORD, StoredStart> starts_;
    std::uint64_t next_sequence_{1};
};

} // namespace peek
