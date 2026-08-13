#include "core/event_store.h"

#include <algorithm>

namespace peek {

EventStore::EventStore(std::size_t capacity) : capacity_(std::max<std::size_t>(1, capacity)) {}

void EventStore::Add(GhostEvent event) {
    std::lock_guard lock(mutex_);
    event.sequence = next_sequence_++;

    if (event.kind == GhostKind::ProcessStarted) {
        starts_[event.process.pid] = StoredStart{event.timestamp, event.process};
    } else if (event.kind == GhostKind::ProcessExited) {
        const auto found = starts_.find(event.process.pid);
        if (found != starts_.end()) {
            event.lifetime = std::chrono::duration_cast<std::chrono::milliseconds>(event.timestamp - found->second.timestamp);
            event.has_lifetime = true;
            const auto& started = found->second.process;
            if (event.process.exe_path.empty()) event.process.exe_path = started.exe_path;
            if (event.process.command_line.empty()) event.process.command_line = started.command_line;
            if (event.process.parent_name.empty()) event.process.parent_name = started.parent_name;
            if (event.process.parent_path.empty()) event.process.parent_path = started.parent_path;
            if (!event.process.parent_pid) event.process.parent_pid = started.parent_pid;
            starts_.erase(found);
        }
    }

    events_.push_front(std::move(event));
    while (events_.size() > capacity_) {
        events_.pop_back();
    }
}

std::vector<GhostEvent> EventStore::Snapshot() const {
    std::lock_guard lock(mutex_);
    return {events_.begin(), events_.end()};
}

std::optional<GhostEvent> EventStore::Latest() const {
    std::lock_guard lock(mutex_);
    if (events_.empty()) return std::nullopt;
    return events_.front();
}

void EventStore::Clear() {
    std::lock_guard lock(mutex_);
    events_.clear();
    starts_.clear();
}

} // namespace peek
