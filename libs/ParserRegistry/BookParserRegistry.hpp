#pragma once

#include <optional>

#include "Domain/BookFormat.hpp"
#include "Domain/ServiceContracts.hpp"
#include "EpubParsing/EpubParser.hpp"
#include "Fb2Parsing/Fb2Parser.hpp"

namespace Librova::ParserRegistry {

class CBookParserRegistry final
{
public:
    [[nodiscard]] static std::optional<Librova::Domain::EBookFormat> TryDetectFormat(const std::filesystem::path& filePath);

    [[nodiscard]] bool CanParse(Librova::Domain::EBookFormat format) const;
    [[nodiscard]] const Librova::Domain::IBookParser& GetParser(Librova::Domain::EBookFormat format) const;
    [[nodiscard]] Librova::Domain::SParsedBook Parse(const std::filesystem::path& filePath) const;

private:
    Librova::EpubParsing::CEpubParser m_epubParser;
    Librova::Fb2Parsing::CFb2Parser m_fb2Parser;
};

} // namespace Librova::ParserRegistry
