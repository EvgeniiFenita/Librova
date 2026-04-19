#include "Foundation/StringUtils.hpp"

#include <algorithm>
#include <cctype>

namespace Librova::Foundation {

std::string ToLower(std::string value)
{
    std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

} // namespace Librova::Foundation
