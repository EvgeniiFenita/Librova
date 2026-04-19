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

std::string CImportDiagnosticText::GetSingleFileLogReason(
    const std::vector<std::string>& warnings,
    const std::string_view error,
    const std::string_view diagnosticError)
{
    return JoinWarningsAndError(
        warnings,
        diagnosticError.empty() ? error : diagnosticError);
}

} // namespace Librova::Importing
