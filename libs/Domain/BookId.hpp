#pragma once

#include <cstdint>

namespace LibriFlow::Domain {

struct SBookId
{
    std::int64_t Value = 0;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return Value > 0;
    }
};

} // namespace LibriFlow::Domain
