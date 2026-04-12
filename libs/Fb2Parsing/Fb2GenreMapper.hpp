#pragma once

#include <string_view>

namespace Librova::Fb2Parsing {

class CFb2GenreMapper final
{
public:
    // Returns the human-readable English display name for the given FB2 2.1 genre code.
    // Returns the raw code unchanged if it is not a recognized FB2 2.1 code.
    [[nodiscard]] static std::string_view ResolveGenreName(std::string_view fb2Code) noexcept;
};

} // namespace Librova::Fb2Parsing
