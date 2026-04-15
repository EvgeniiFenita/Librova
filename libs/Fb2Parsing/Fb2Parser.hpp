#pragma once

#include "Domain/ServiceContracts.hpp"

namespace Librova::Fb2Parsing {

class CFb2Parser final : public Librova::Domain::IBookParser
{
public:
    [[nodiscard]] bool CanParse(Librova::Domain::EBookFormat format) const override;
    [[nodiscard]] Librova::Domain::SParsedBook Parse(
        const std::filesystem::path& filePath,
        std::string_view logicalSourceLabel = {}) const override;
};

} // namespace Librova::Fb2Parsing
