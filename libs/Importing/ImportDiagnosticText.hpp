#pragma once

#include <string>
#include <vector>

namespace Librova::Importing {

class CImportDiagnosticText final
{
public:
    [[nodiscard]] static std::string JoinWarningsAndError(
        const std::vector<std::string>& warnings,
        const std::string& error);
};

} // namespace Librova::Importing
