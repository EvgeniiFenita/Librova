#pragma once

#include <string_view>

namespace Librova::Core {

class CVersion
{
public:
    [[nodiscard]] static std::string_view GetValue() noexcept;
};

} // namespace Librova::Core
