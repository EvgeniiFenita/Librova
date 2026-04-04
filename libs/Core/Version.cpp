#include "Core/Version.hpp"

namespace Librova::Core {

std::string_view CVersion::GetValue() noexcept
{
    return LIBROVA_PRODUCT_VERSION;
}

} // namespace Librova::Core
