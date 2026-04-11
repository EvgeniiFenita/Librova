#include "Application/ImportWorkloadPlanner.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>

#include "ImportSourceExpander/ImportDiagnostics.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace {

[[nodiscard]] std::string ToLower(std::string value)
{
    std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

[[nodiscard]] bool IsZipStandalonePath(const std::filesystem::path& path)
{
    return ToLower(path.extension().string()) == ".zip";
}

} // namespace

namespace Librova::Application {

CImportWorkloadPlanner::CImportWorkloadPlanner(const Librova::ZipImporting::CZipImportCoordinator& zipImportCoordinator)
    : m_zipImportCoordinator(zipImportCoordinator)
{
}

SPreparedImportWorkload CImportWorkloadPlanner::Prepare(
    const std::vector<std::filesystem::path>& candidates,
    const Librova::Domain::IProgressSink& progressSink,
    const std::stop_token stopToken) const
{
    SPreparedImportWorkload workload;

    for (const auto& sourcePath : candidates)
    {
        if (stopToken.stop_requested() || progressSink.IsCancellationRequested())
        {
            break;
        }

        if (IsZipStandalonePath(sourcePath))
        {
            try
            {
                const auto plannedEntries = m_zipImportCoordinator.CountPlannedEntries(sourcePath, &progressSink, stopToken);
                workload.PlannedEntriesBySource.emplace(sourcePath, plannedEntries);
                workload.TotalEntries += plannedEntries;
            }
            catch (const std::exception& error)
            {
                workload.PlannedEntriesBySource.emplace(sourcePath, 1);
                ++workload.TotalEntries;
                const auto warning =
                    "Failed to inspect ZIP archive '" + Librova::Unicode::PathToUtf8(sourcePath) + "': " + error.what();
                workload.Warnings.push_back(warning);
                Librova::ImportSourceExpander::LogImportSourceIssueIfInitialized(
                    sourcePath,
                    "zip-inspection",
                    "failed",
                    "source-inspection-failed",
                    error.what());
            }
        }
        else
        {
            ++workload.TotalEntries;
        }
    }

    return workload;
}

} // namespace Librova::Application
