#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Librova::ImportSourceExpander {

struct SExpandedImportSources
{
    std::vector<std::filesystem::path> Candidates;
    std::vector<std::string> Warnings;
};

class CImportSourceExpander final
{
public:
    [[nodiscard]] static SExpandedImportSources Expand(const std::vector<std::filesystem::path>& sourcePaths);
    [[nodiscard]] static bool IsSupportedStandaloneImportPath(const std::filesystem::path& path);
    [[nodiscard]] static bool HasSupportedFiles(const std::filesystem::path& dirPath);
    [[nodiscard]] static std::string BuildSupportedImportSourcesMessage();
};

} // namespace Librova::ImportSourceExpander
