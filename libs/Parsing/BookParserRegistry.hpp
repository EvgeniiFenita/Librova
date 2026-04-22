#pragma once

#include <optional>

#include "Domain/BookFormat.hpp"
#include "Domain/ServiceContracts.hpp"
#include "Parsing/EpubParser.hpp"
#include "Parsing/Fb2Parser.hpp"

namespace Librova::ParserRegistry {

class CBookParserRegistry final
{
public:
    [[nodiscard]] static std::optional<Librova::Domain::EBookFormat> TryDetectFormat(const std::filesystem::path& filePath);

    [[nodiscard]] bool CanParse(Librova::Domain::EBookFormat format) const;
    [[nodiscard]] const Librova::Domain::IBookParser& GetParser(Librova::Domain::EBookFormat format) const;
    [[nodiscard]] Librova::Domain::SParsedBook Parse(
        const std::filesystem::path& filePath,
        std::string_view logicalSourceLabel = {},
        const Librova::Domain::SBookParseOptions& options = {}) const;

private:
    Librova::EpubParsing::CEpubParser m_epubParser;
    Librova::Fb2Parsing::CFb2Parser m_fb2Parser;
};

} // namespace Librova::ParserRegistry
