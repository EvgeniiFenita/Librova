#include "ImportSourceExpander/ImportSourceExpander.hpp"

#include <algorithm>
#include <cctype>
#include <set>

#include "ImportSourceExpander/ImportDiagnostics.hpp"
#include "Foundation/UnicodeConversion.hpp"

namespace {

[[nodiscard]] std::string ToLower(std::string value)
{
    std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

} // namespace

namespace Librova::ImportSourceExpander {

SExpandedImportSources CImportSourceExpander::Expand(const std::vector<std::filesystem::path>& sourcePaths)
{
    SExpandedImportSources expanded;
    std::set<std::string> seenPaths;

    for (const auto& rawSourcePath : sourcePaths)
    {
        const auto sourcePath = rawSourcePath.lexically_normal();
        if (sourcePath.empty())
        {
            continue;
        }

        std::error_code errorCode;
        const bool isDirectory = std::filesystem::is_directory(sourcePath, errorCode);
        if (isDirectory)
        {
            std::size_t discoveredSupportedEntries = 0;
            for (std::filesystem::recursive_directory_iterator iterator(
                     sourcePath,
                     std::filesystem::directory_options::skip_permission_denied,
                     errorCode),
                 end;
                 iterator != end;
                 iterator.increment(errorCode))
            {
                if (errorCode)
                {
                    expanded.Warnings.push_back(
                        "Failed to enumerate directory '" + Librova::Unicode::PathToUtf8(sourcePath) + "'.");
                    break;
                }

                if (!iterator->is_regular_file())
                {
                    continue;
                }

                const auto candidatePath = iterator->path().lexically_normal();
                if (!IsSupportedStandaloneImportPath(candidatePath))
                {
                    continue;
                }

                const auto candidateKey = Librova::Unicode::PathToUtf8(candidatePath);
                if (!seenPaths.insert(candidateKey).second)
                {
                    continue;
                }

                expanded.Candidates.push_back(candidatePath);
                ++discoveredSupportedEntries;
            }

            if (discoveredSupportedEntries == 0)
            {
                const auto warning =
                    "Directory '" + Librova::Unicode::PathToUtf8(sourcePath) + "' does not contain supported .fb2, .epub, or .zip files.";
                expanded.Warnings.push_back(warning);
                LogImportSourceIssueIfInitialized(
                    sourcePath,
                    "source-selection",
                    "skipped",
                    "empty-directory",
                    warning);
            }

            continue;
        }

        if (std::filesystem::is_regular_file(sourcePath, errorCode))
        {
            if (!IsSupportedStandaloneImportPath(sourcePath))
            {
                const auto warning =
                    "Unsupported selected source '" + Librova::Unicode::PathToUtf8(sourcePath) + "'. Only .fb2, .epub, and .zip are supported.";
                expanded.Warnings.push_back(warning);
                LogImportSourceIssueIfInitialized(
                    sourcePath,
                    "source-selection",
                    "skipped",
                    "unsupported-source",
                    warning);
                continue;
            }

            const auto sourceKey = Librova::Unicode::PathToUtf8(sourcePath);
            if (seenPaths.insert(sourceKey).second)
            {
                expanded.Candidates.push_back(sourcePath);
            }

            continue;
        }

        const auto warning = "Selected import source '" + Librova::Unicode::PathToUtf8(sourcePath) + "' does not exist.";
        expanded.Warnings.push_back(warning);
        LogImportSourceIssueIfInitialized(
            sourcePath,
            "source-selection",
            "failed",
            "missing-source",
            warning);
    }

    return expanded;
}

bool CImportSourceExpander::IsSupportedStandaloneImportPath(const std::filesystem::path& path)
{
    const auto extension = ToLower(path.extension().string());
    return extension == ".fb2" || extension == ".epub" || extension == ".zip";
}

std::string CImportSourceExpander::BuildSupportedImportSourcesMessage()
{
    return "Supported source types are .fb2, .epub, and .zip, or a directory containing them.";
}

bool CImportSourceExpander::HasSupportedFiles(const std::filesystem::path& dirPath)
{
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(
        dirPath, std::filesystem::directory_options::skip_permission_denied, ec);
    for (; !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
    {
        if (it->is_regular_file() && IsSupportedStandaloneImportPath(it->path()))
        {
            return true;
        }
    }
    return false;
}

} // namespace Librova::ImportSourceExpander
