#pragma once

#include <cstdint>

namespace Librova::Domain {

struct SBookId
{
    std::int64_t Value = 0;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return Value > 0;
    }
};

} // namespace Librova::Domain
