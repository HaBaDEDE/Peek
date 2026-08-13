#include "core/icon_cache.h"

#include <shellapi.h>

namespace peek {

IconCache::IconCache(std::size_t capacity) : capacity_(capacity) {}

IconCache::~IconCache() {
    for (auto& [_, entry] : entries_) if (entry.icon) DestroyIcon(entry.icon);
}

HICON IconCache::Get(const std::wstring& exe_path) {
    std::lock_guard lock(mutex_);
    if (auto found = entries_.find(exe_path); found != entries_.end()) {
        order_.splice(order_.begin(), order_, found->second.order);
        return found->second.icon;
    }
    SHFILEINFOW shell{};
    if (exe_path.empty() || !SHGetFileInfoW(exe_path.c_str(), 0, &shell, sizeof(shell), SHGFI_ICON | SHGFI_SMALLICON)) {
        return LoadIconW(nullptr, IDI_APPLICATION);
    }
    order_.push_front(exe_path);
    entries_.emplace(exe_path, Entry{shell.hIcon, order_.begin()});
    while (entries_.size() > capacity_) {
        const std::wstring key = order_.back();
        auto item = entries_.find(key);
        if (item != entries_.end()) {
            if (item->second.icon) DestroyIcon(item->second.icon);
            entries_.erase(item);
        }
        order_.pop_back();
    }
    return shell.hIcon;
}

} // namespace peek
