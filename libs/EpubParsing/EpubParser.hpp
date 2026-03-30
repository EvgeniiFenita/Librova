#pragma once

#include "Domain/ServiceContracts.hpp"

namespace Librova::EpubParsing {

class CEpubParser final : public Librova::Domain::IBookParser
{
public:
    [[nodiscard]] bool CanParse(Librova::Domain::EBookFormat format) const override;
    [[nodiscard]] Librova::Domain::SParsedBook Parse(const std::filesystem::path& filePath) const override;
};

} // namespace Librova::EpubParsing
