#include "Domain/StorageEncoding.hpp"

namespace Librova::Domain {

std::string_view ToString(const EStorageEncoding encoding) noexcept
{
    switch (encoding)
    {
    case EStorageEncoding::Plain:
        return "plain";
    case EStorageEncoding::Compressed:
        return "compressed";
    }

    return "unknown";
}

std::optional<EStorageEncoding> TryParseStorageEncoding(const std::string_view value) noexcept
{
    if (value == "plain" || value == "PLAIN")
    {
        return EStorageEncoding::Plain;
    }

    if (value == "compressed" || value == "COMPRESSED")
    {
        return EStorageEncoding::Compressed;
    }

    return std::nullopt;
}

} // namespace Librova::Domain
