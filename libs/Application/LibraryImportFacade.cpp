#include "Application/LibraryImportFacade.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <stdexcept>

namespace {

[[nodiscard]] std::string ToLower(std::string value)
{
    std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

[[nodiscard]] bool IsSupportedStandaloneImportPath(const std::filesystem::path& path)
{
    const auto extension = ToLower(path.extension().string());
    return extension == ".fb2" || extension == ".epub" || extension == ".zip";
}

[[nodiscard]] bool IsZipStandalonePath(const std::filesystem::path& path)
{
    return ToLower(path.extension().string()) == ".zip";
}

[[nodiscard]] std::string PathToUtf8(const std::filesystem::path& path)
{
    const auto value = path.generic_u8string();
    return {
        reinterpret_cast<const char*>(value.data()),
        value.size()
    };
}

} // namespace

namespace Librova::Application {

CLibraryImportFacade::CLibraryImportFacade(
    const Librova::Importing::ISingleFileImporter& singleFileImporter,
    const Librova::ZipImporting::CZipImportCoordinator& zipImportCoordinator)
    : m_singleFileImporter(singleFileImporter)
    , m_zipImportCoordinator(zipImportCoordinator)
{
}

bool CLibraryImportFacade::IsZipPath(const std::filesystem::path& path)
{
    return ToLower(path.extension().string()) == ".zip";
}

namespace {

struct SExpandedImportSources
{
    std::vector<std::filesystem::path> Candidates;
    std::vector<std::string> Warnings;
};

class CScopedStructuredProgressSink final : public Librova::Domain::IProgressSink, public Librova::Domain::IStructuredImportProgressSink
{
public:
    CScopedStructuredProgressSink(
        Librova::Domain::IProgressSink& innerSink,
        const std::size_t totalEntries,
        const std::size_t processedEntries,
        const std::size_t contributionEntries)
        : m_innerSink(innerSink)
        , m_totalEntries(totalEntries)
        , m_processedEntries(processedEntries)
        , m_contributionEntries(std::max<std::size_t>(contributionEntries, 1))
    {
    }

    void ReportValue(const int percent, std::string_view message) override
    {
        m_innerSink.ReportValue(MapPercent(percent), message);
    }

    bool IsCancellationRequested() const override
    {
        return m_innerSink.IsCancellationRequested();
    }

    void ReportStructuredProgress(
        const std::size_t totalEntries,
        const std::size_t processedEntries,
        const std::size_t importedEntries,
        const std::size_t failedEntries,
        const std::size_t skippedEntries,
        const int percent,
        std::string_view message) override
    {
        if (auto* structuredSink = dynamic_cast<Librova::Domain::IStructuredImportProgressSink*>(&m_innerSink); structuredSink != nullptr)
        {
            structuredSink->ReportStructuredProgress(
                totalEntries,
                m_processedEntries + processedEntries,
                importedEntries,
                failedEntries,
                skippedEntries,
                MapPercent(percent),
                message);
            return;
        }

        m_innerSink.ReportValue(MapPercent(percent), message);
    }

private:
    [[nodiscard]] int MapPercent(const int localPercent) const noexcept
    {
        const auto clampedLocalPercent = std::clamp(localPercent, 0, 100);
        const auto scaledProgress = static_cast<long long>(m_processedEntries * 100)
            + (static_cast<long long>(m_contributionEntries) * clampedLocalPercent);
        const auto denominator = static_cast<long long>(std::max<std::size_t>(m_totalEntries, 1)) * 100LL;
        return static_cast<int>(std::clamp((scaledProgress * 100LL) / denominator, 0LL, 100LL));
    }

    Librova::Domain::IProgressSink& m_innerSink;
    std::size_t m_totalEntries = 0;
    std::size_t m_processedEntries = 0;
    std::size_t m_contributionEntries = 1;
};

[[nodiscard]] SExpandedImportSources ExpandImportSources(const Librova::Application::SImportRequest& request)
{
    SExpandedImportSources expanded;
    std::set<std::string> seenPaths;

    for (const auto& rawSourcePath : request.SourcePaths)
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
                        "Failed to enumerate directory '" + PathToUtf8(sourcePath) + "'.");
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

                const auto candidateKey = PathToUtf8(candidatePath);
                if (!seenPaths.insert(candidateKey).second)
                {
                    continue;
                }

                expanded.Candidates.push_back(candidatePath);
                ++discoveredSupportedEntries;
            }

            if (discoveredSupportedEntries == 0)
            {
                expanded.Warnings.push_back(
                    "Directory '" + PathToUtf8(sourcePath) + "' does not contain supported .fb2, .epub, or .zip files.");
            }

            continue;
        }

        if (std::filesystem::is_regular_file(sourcePath, errorCode))
        {
            if (!IsSupportedStandaloneImportPath(sourcePath))
            {
                expanded.Warnings.push_back(
                    "Unsupported selected source '" + PathToUtf8(sourcePath) + "'. Only .fb2, .epub, and .zip are supported.");
                continue;
            }

            const auto sourceKey = PathToUtf8(sourcePath);
            if (seenPaths.insert(sourceKey).second)
            {
                expanded.Candidates.push_back(sourcePath);
            }

            continue;
        }

        expanded.Warnings.push_back(
            "Selected import source '" + PathToUtf8(sourcePath) + "' does not exist.");
    }

    return expanded;
}

[[nodiscard]] Librova::Application::EImportMode DetermineImportMode(
    const Librova::Application::SImportRequest& request,
    const std::vector<std::filesystem::path>& candidates)
{
    if (request.SourcePaths.size() != 1)
    {
        return Librova::Application::EImportMode::Batch;
    }

    if (request.SourcePaths.front().empty())
    {
        return Librova::Application::EImportMode::Batch;
    }

    std::error_code errorCode;
    if (std::filesystem::is_directory(request.SourcePaths.front(), errorCode))
    {
        return Librova::Application::EImportMode::Batch;
    }

    if (candidates.size() == 1 && IsSupportedStandaloneImportPath(candidates.front()) && ToLower(candidates.front().extension().string()) == ".zip")
    {
        return Librova::Application::EImportMode::ZipArchive;
    }

    return candidates.size() == 1
        ? Librova::Application::EImportMode::SingleFile
        : Librova::Application::EImportMode::Batch;
}

void MergeWarnings(std::vector<std::string>& target, const std::vector<std::string>& source)
{
    target.insert(target.end(), source.begin(), source.end());
}

[[nodiscard]] std::size_t CountPlannedEntries(
    const std::vector<std::filesystem::path>& candidates,
    const Librova::ZipImporting::CZipImportCoordinator& zipImportCoordinator)
{
    std::size_t totalEntries = 0;

    for (const auto& sourcePath : candidates)
    {
        if (IsZipStandalonePath(sourcePath))
        {
            totalEntries += zipImportCoordinator.CountPlannedEntries(sourcePath);
        }
        else
        {
            ++totalEntries;
        }
    }

    return totalEntries;
}

} // namespace

SImportResult CLibraryImportFacade::Run(
    const SImportRequest& request,
    Librova::Domain::IProgressSink& progressSink,
    const std::stop_token stopToken) const
{
    if (!request.IsValid())
    {
        throw std::invalid_argument("Import request must contain source path and working directory.");
    }

    const auto expandedSources = ExpandImportSources(request);
    SImportResult result;
    result.Summary.Mode = DetermineImportMode(request, expandedSources.Candidates);
    result.Summary.Warnings = expandedSources.Warnings;
    result.Summary.TotalEntries = CountPlannedEntries(expandedSources.Candidates, m_zipImportCoordinator);
    std::size_t processedEntries = 0;

    if (auto* structuredSink = dynamic_cast<Librova::Domain::IStructuredImportProgressSink*>(&progressSink); structuredSink != nullptr)
    {
        structuredSink->ReportStructuredProgress(
            result.Summary.TotalEntries,
            0,
            0,
            0,
            0,
            0,
            result.Summary.TotalEntries == 0 ? "No supported import entries were found." : "Prepared import workload");
    }

    for (const auto& sourcePath : expandedSources.Candidates)
    {
        if (IsZipPath(sourcePath))
        {
            const auto zipEntryCount = m_zipImportCoordinator.CountPlannedEntries(sourcePath);
            CScopedStructuredProgressSink zipProgressSink(progressSink, result.Summary.TotalEntries, processedEntries, zipEntryCount);
            const auto zipResult = m_zipImportCoordinator.Run({
                .ZipPath = sourcePath,
                .WorkingDirectory = request.WorkingDirectory,
                .AllowProbableDuplicates = request.AllowProbableDuplicates
            }, zipProgressSink, stopToken);

            result.Summary.ImportedEntries += zipResult.ImportedCount();
            processedEntries += static_cast<std::size_t>(std::count_if(
                zipResult.Entries.begin(),
                zipResult.Entries.end(),
                [](const auto& entry) {
                    return entry.Status != Librova::ZipImporting::EZipEntryImportStatus::Cancelled;
                }));

            if (result.Summary.Mode == EImportMode::ZipArchive)
            {
                result.ZipResult = zipResult;
            }

            for (const auto& entry : zipResult.Entries)
            {
                switch (entry.Status)
                {
                case Librova::ZipImporting::EZipEntryImportStatus::Imported:
                    break;
                case Librova::ZipImporting::EZipEntryImportStatus::UnsupportedEntry:
                case Librova::ZipImporting::EZipEntryImportStatus::NestedArchiveSkipped:
                    ++result.Summary.SkippedEntries;
                    if (!entry.Error.empty())
                    {
                        result.Summary.Warnings.push_back(entry.Error);
                    }
                    break;
                case Librova::ZipImporting::EZipEntryImportStatus::Failed:
                    ++result.Summary.FailedEntries;
                    if (entry.SingleFileResult.has_value())
                    {
                        MergeWarnings(result.Summary.Warnings, entry.SingleFileResult->Warnings);
                        if (!entry.SingleFileResult->Error.empty())
                        {
                            result.Summary.Warnings.push_back(entry.SingleFileResult->Error);
                        }
                    }
                    else if (!entry.Error.empty())
                    {
                        result.Summary.Warnings.push_back(entry.Error);
                    }
                    break;
                case Librova::ZipImporting::EZipEntryImportStatus::Cancelled:
                    if (entry.SingleFileResult.has_value())
                    {
                        MergeWarnings(result.Summary.Warnings, entry.SingleFileResult->Warnings);
                        if (!entry.SingleFileResult->Error.empty())
                        {
                            result.Summary.Warnings.push_back(entry.SingleFileResult->Error);
                        }
                    }
                    else if (!entry.Error.empty())
                    {
                        result.Summary.Warnings.push_back(entry.Error);
                    }
                    break;
                }
            }

            if (auto* structuredSink = dynamic_cast<Librova::Domain::IStructuredImportProgressSink*>(&progressSink); structuredSink != nullptr)
            {
                const auto percent = static_cast<int>((processedEntries * 100) / std::max<std::size_t>(result.Summary.TotalEntries, 1));
                structuredSink->ReportStructuredProgress(
                    result.Summary.TotalEntries,
                    processedEntries,
                    result.Summary.ImportedEntries,
                    result.Summary.FailedEntries,
                    result.Summary.SkippedEntries,
                    percent,
                    "Processed archive source");
            }

            continue;
        }

        CScopedStructuredProgressSink singleProgressSink(progressSink, result.Summary.TotalEntries, processedEntries, 1);
        const auto singleFileResult = m_singleFileImporter.Run({
            .SourcePath = sourcePath,
            .WorkingDirectory = request.WorkingDirectory,
            .Sha256Hex = expandedSources.Candidates.size() == 1 ? request.Sha256Hex : std::nullopt,
            .AllowProbableDuplicates = request.AllowProbableDuplicates
        }, singleProgressSink, stopToken);

        ++processedEntries;

        if (result.Summary.Mode == EImportMode::SingleFile)
        {
            result.SingleFileResult = singleFileResult;
        }

        if (singleFileResult.IsSuccess())
        {
            ++result.Summary.ImportedEntries;
            if (auto* structuredSink = dynamic_cast<Librova::Domain::IStructuredImportProgressSink*>(&progressSink); structuredSink != nullptr)
            {
                const auto percent = static_cast<int>((processedEntries * 100) / std::max<std::size_t>(result.Summary.TotalEntries, 1));
                structuredSink->ReportStructuredProgress(
                    result.Summary.TotalEntries,
                    processedEntries,
                    result.Summary.ImportedEntries,
                    result.Summary.FailedEntries,
                    result.Summary.SkippedEntries,
                    percent,
                    "Processed source file");
            }
            continue;
        }

        switch (singleFileResult.Status)
        {
        case Librova::Importing::ESingleFileImportStatus::RejectedDuplicate:
        case Librova::Importing::ESingleFileImportStatus::DecisionRequired:
        case Librova::Importing::ESingleFileImportStatus::UnsupportedFormat:
            ++result.Summary.SkippedEntries;
            break;
        case Librova::Importing::ESingleFileImportStatus::Cancelled:
        case Librova::Importing::ESingleFileImportStatus::Failed:
            ++result.Summary.FailedEntries;
            break;
        case Librova::Importing::ESingleFileImportStatus::Imported:
            break;
        }

        MergeWarnings(result.Summary.Warnings, singleFileResult.Warnings);
        if (!singleFileResult.Error.empty())
        {
            result.Summary.Warnings.push_back(singleFileResult.Error);
        }

        if (auto* structuredSink = dynamic_cast<Librova::Domain::IStructuredImportProgressSink*>(&progressSink); structuredSink != nullptr)
        {
            const auto percent = static_cast<int>((processedEntries * 100) / std::max<std::size_t>(result.Summary.TotalEntries, 1));
            structuredSink->ReportStructuredProgress(
                result.Summary.TotalEntries,
                processedEntries,
                result.Summary.ImportedEntries,
                result.Summary.FailedEntries,
                result.Summary.SkippedEntries,
                percent,
                "Processed source file");
        }
    }

    if (expandedSources.Candidates.empty())
    {
        if (request.SourcePaths.size() == 1)
        {
            std::error_code errorCode;
            result.Summary.Mode = std::filesystem::is_directory(request.SourcePaths.front(), errorCode)
                ? EImportMode::Batch
                : EImportMode::SingleFile;
        }
        else
        {
            result.Summary.Mode = EImportMode::Batch;
        }
    }

    return result;
}

} // namespace Librova::Application
