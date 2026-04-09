#include "Domain/BookFormat.hpp"

namespace Librova::Domain {

std::string_view ToString(const EBookFormat format) noexcept
{
    switch (format)
    {
    case EBookFormat::Epub:
        return "epub";
    case EBookFormat::Fb2:
        return "fb2";
    }

    return "unknown";
}

std::string_view GetManagedFileName(const EBookFormat format) noexcept
{
    switch (format)
    {
    case EBookFormat::Epub:
        return "book.epub";
    case EBookFormat::Fb2:
        return "book.fb2";
    }

    return "book.dat";
}

std::string_view GetManagedFileName(const EBookFormat format, const EStorageEncoding encoding) noexcept
{
    if (format == EBookFormat::Fb2 && encoding == EStorageEncoding::Compressed)
    {
        return "book.fb2.gz";
    }

    return GetManagedFileName(format);
}

std::optional<EBookFormat> TryParseBookFormat(const std::string_view value) noexcept
{
    if (value == "epub" || value == ".epub" || value == "EPUB")
    {
        return EBookFormat::Epub;
    }

    if (value == "fb2" || value == ".fb2" || value == "FB2")
    {
        return EBookFormat::Fb2;
    }

    return std::nullopt;
}

} // namespace Librova::Domain
