#pragma once

#include <optional>

#include "Domain/BookFormat.hpp"
#include "Domain/ServiceContracts.hpp"
#include "EpubParsing/EpubParser.hpp"
#include "Fb2Parsing/Fb2Parser.hpp"

namespace LibriFlow::ParserRegistry {

class CBookParserRegistry final
{
public:
    [[nodiscard]] static std::optional<LibriFlow::Domain::EBookFormat> TryDetectFormat(const std::filesystem::path& filePath);

    [[nodiscard]] bool CanParse(LibriFlow::Domain::EBookFormat format) const;
    [[nodiscard]] const LibriFlow::Domain::IBookParser& GetParser(LibriFlow::Domain::EBookFormat format) const;
    [[nodiscard]] LibriFlow::Domain::SParsedBook Parse(const std::filesystem::path& filePath) const;

private:
    LibriFlow::EpubParsing::CEpubParser m_epubParser;
    LibriFlow::Fb2Parsing::CFb2Parser m_fb2Parser;
};

} // namespace LibriFlow::ParserRegistry
