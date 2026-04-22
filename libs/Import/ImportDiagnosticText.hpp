#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Librova::Importing {

class CImportDiagnosticText final
{
public:
    [[nodiscard]] static std::string JoinWarningsAndError(
        const std::vector<std::string>& warnings,
        std::string_view error);

    [[nodiscard]] static std::string GetSingleFileLogReason(
        const std::vector<std::string>& warnings,
        std::string_view error,
        std::string_view diagnosticError);
};

} // namespace Librova::Importing
