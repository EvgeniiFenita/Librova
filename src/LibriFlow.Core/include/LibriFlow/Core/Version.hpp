#pragma once

#include <string_view>

namespace LibriFlow::Core {

class CVersion
{
public:
    [[nodiscard]] static std::string_view GetValue() noexcept;
};

} // namespace LibriFlow::Core
