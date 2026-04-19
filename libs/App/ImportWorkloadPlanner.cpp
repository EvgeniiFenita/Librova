#include "App/ImportWorkloadPlanner.hpp"

#include <string_view>

#include "Import/ImportDiagnostics.hpp"
#include "Foundation/StringUtils.hpp"
#include "Foundation/UnicodeConversion.hpp"

namespace {

[[nodiscard]] bool IsZipStandalonePath(const std::filesystem::path& path)
{
    return Librova::Foundation::ToLower(path.extension().string()) == ".zip";
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
