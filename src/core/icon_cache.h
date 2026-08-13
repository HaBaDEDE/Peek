#pragma once

#include <windows.h>

#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

namespace peek {

class IconCache {
public:
    explicit IconCache(std::size_t capacity = 64);
    ~IconCache();
    HICON Get(const std::wstring& exe_path);

private:
    std::size_t capacity_;
    std::mutex mutex_;
    std::list<std::wstring> order_;
    struct Entry { HICON icon{}; std::list<std::wstring>::iterator order; };
    std::unordered_map<std::wstring, Entry> entries_;
};

} // namespace peek
