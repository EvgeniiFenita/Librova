#pragma once

#include "Domain/ServiceContracts.hpp"

namespace LibriFlow::Fb2Parsing {

class CFb2Parser final : public LibriFlow::Domain::IBookParser
{
public:
    [[nodiscard]] bool CanParse(LibriFlow::Domain::EBookFormat format) const override;
    [[nodiscard]] LibriFlow::Domain::SParsedBook Parse(const std::filesystem::path& filePath) const override;
};

} // namespace LibriFlow::Fb2Parsing
