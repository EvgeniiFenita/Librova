#include "Foundation/FileSystemUtils.hpp"

#include <stdexcept>
#include <string>
#include <system_error>

#include "Foundation/UnicodeConversion.hpp"

namespace Librova::Foundation {

void EnsureDirectory(const std::filesystem::path& path)
{
    std::error_code errorCode;
    std::filesystem::create_directories(path, errorCode);

    if (errorCode)
    {
        throw std::runtime_error(
            std::string{"Failed to create directory: "} + Librova::Unicode::PathToUtf8(path)
            + ": " + errorCode.message());
    }
}

void RemovePathNoThrow(const std::filesystem::path& path) noexcept
{
    if (path.empty())
    {
        return;
    }

    std::error_code errorCode;
    std::filesystem::remove_all(path, errorCode);
}

} // namespace Librova::Foundation
