#include "Parsing/BookParserRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

#include "Parsing/EpubParser.hpp"
#include "Parsing/Fb2Parser.hpp"

namespace Librova::ParserRegistry {
namespace {

[[nodiscard]] std::string NormalizeExtension(std::string extension)
{
    std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });

    return extension;
}

} // namespace

std::optional<Librova::Domain::EBookFormat> CBookParserRegistry::TryDetectFormat(const std::filesystem::path& filePath)
{
    const std::string extension = NormalizeExtension(filePath.extension().string());

    if (extension == ".epub")
    {
        return Librova::Domain::EBookFormat::Epub;
    }

    if (extension == ".fb2")
    {
        return Librova::Domain::EBookFormat::Fb2;
    }

    return std::nullopt;
}

bool CBookParserRegistry::CanParse(const Librova::Domain::EBookFormat format) const
{
    return m_epubParser.CanParse(format) || m_fb2Parser.CanParse(format);
}

const Librova::Domain::IBookParser& CBookParserRegistry::GetParser(const Librova::Domain::EBookFormat format) const
{
    if (m_epubParser.CanParse(format))
    {
        return m_epubParser;
    }

    if (m_fb2Parser.CanParse(format))
    {
        return m_fb2Parser;
    }

    throw std::runtime_error("No parser is registered for the requested format.");
}

Librova::Domain::SParsedBook CBookParserRegistry::Parse(
    const std::filesystem::path& filePath,
    const std::string_view logicalSourceLabel) const
{
    const std::optional<Librova::Domain::EBookFormat> format = TryDetectFormat(filePath);

    if (!format.has_value())
    {
        throw std::runtime_error("Unable to detect parser format from file extension.");
    }

    return GetParser(*format).Parse(filePath, logicalSourceLabel);
}

} // namespace Librova::ParserRegistry
