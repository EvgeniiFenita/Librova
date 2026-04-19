#include "Import/ImportDiagnosticText.hpp"

namespace Librova::Importing {

std::string CImportDiagnosticText::JoinWarningsAndError(
    const std::vector<std::string>& warnings,
    const std::string_view error)
{
    std::string combined;

    for (const auto& warning : warnings)
    {
        if (warning.empty())
        {
            continue;
        }

        if (!combined.empty())
        {
            combined += " | ";
        }

        combined += warning;
    }

    if (!error.empty())
    {
        if (!combined.empty())
        {
            combined += " | ";
        }

        combined += error;
    }

    return combined;
}

} // namespace Librova::Importing
