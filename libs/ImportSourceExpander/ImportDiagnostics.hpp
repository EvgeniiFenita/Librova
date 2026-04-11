#pragma once

#include <filesystem>
#include <string_view>

#include "Logging/Logging.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace Librova::ImportSourceExpander {

inline void LogImportSourceIssueIfInitialized(
    const std::filesystem::path& sourcePath,
    std::string_view stage,
    std::string_view outcome,
    std::string_view status,
    std::string_view reason)
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    const auto utf8SourcePath = Librova::Unicode::PathToUtf8(sourcePath);
    if (outcome == "failed")
    {
        Librova::Logging::Error(
            "Import source failed: source='{}' stage='{}' outcome='{}' status='{}' reason='{}'",
            utf8SourcePath,
            stage,
            outcome,
            status,
            reason);
        return;
    }

    Librova::Logging::Warn(
        "Import source skipped: source='{}' stage='{}' outcome='{}' status='{}' reason='{}'",
        utf8SourcePath,
        stage,
        outcome,
        status,
        reason);
}

} // namespace Librova::ImportSourceExpander
