#pragma once

#include "core/process_resolver.h"
#include "core/types.h"

#include <string>
#include <vector>

namespace peek {

class UnlockService {
public:
    explicit UnlockService(ProcessResolver& resolver) : resolver_(resolver) {}
    [[nodiscard]] std::vector<LockOwner> Query(const std::wstring& file_path, std::wstring& error) const;

private:
    ProcessResolver& resolver_;
};

} // namespace peek
