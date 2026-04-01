#include "Application/LibraryImportFacade.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

#include "Logging/Logging.hpp"
#include "ManagedPaths/ManagedPathSafety.hpp"

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

[[nodiscard]] std::string JoinWarningsAndError(
    const std::vector<std::string>& warnings,
    const std::string& error)
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

void LogImportSourceIssueIfInitialized(
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

    const auto utf8SourcePath = PathToUtf8(sourcePath);
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

[[nodiscard]] bool IsSafeRelativeManagedPath(const std::filesystem::path& path)
{
    if (path.empty() || path.is_absolute())
    {
        return false;
    }

    const auto normalized = path.lexically_normal();
    if (normalized.empty() || normalized.is_absolute())
    {
        return false;
    }

    for (const auto& part : normalized)
    {
        if (part == "..")
        {
            return false;
        }
    }

    return true;
}

[[nodiscard]] std::optional<std::filesystem::path> TryResolveManagedPathWithinRoot(
    const std::filesystem::path& root,
    const std::filesystem::path& managedPath)
{
    if (managedPath.empty())
    {
        return std::nullopt;
    }

    const auto normalizedManagedPath = managedPath.lexically_normal();
    std::filesystem::path candidatePath;

    if (normalizedManagedPath.is_absolute())
    {
        candidatePath = normalizedManagedPath;
    }
    else
    {
        if (!IsSafeRelativeManagedPath(normalizedManagedPath))
        {
            throw std::runtime_error("Managed rollback path is unsafe.");
        }

        candidatePath = (root / normalizedManagedPath).lexically_normal();
    }

    if (!std::filesystem::exists(candidatePath))
    {
        return std::nullopt;
    }

    return Librova::ManagedPaths::ResolveExistingPathWithinRoot(
        root,
        managedPath,
        "Managed rollback path does not exist.",
        "Managed rollback path is unsafe.",
        "Managed rollback path could not be canonicalized.");
}

void CleanupEmptyParentsNoThrow(const std::filesystem::path& path, const std::filesystem::path& stopRoot) noexcept
{
    std::error_code errorCode;
    auto current = path.parent_path();
    const auto normalizedStopRoot = stopRoot.lexically_normal();

    while (!current.empty() && current.lexically_normal() != normalizedStopRoot)
    {
        if (!std::filesystem::remove(current, errorCode) || errorCode)
        {
            break;
        }

        current = current.parent_path();
    }
}

void RemoveManagedPathNoThrow(
    const std::filesystem::path& root,
    const std::filesystem::path& managedPath) noexcept
{
    try
    {
        const auto resolvedPath = TryResolveManagedPathWithinRoot(root, managedPath);
        if (!resolvedPath.has_value())
        {
            return;
        }

        std::error_code errorCode;
        std::filesystem::remove(*resolvedPath, errorCode);
        if (!errorCode)
        {
            CleanupEmptyParentsNoThrow(*resolvedPath, root);
        }
    }
    catch (...)
    {
    }
}

} // namespace

namespace Librova::Application {

CLibraryImportFacade::CLibraryImportFacade(
    const Librova::Importing::ISingleFileImporter& singleFileImporter,
    const Librova::ZipImporting::CZipImportCoordinator& zipImportCoordinator,
    Librova::Domain::IBookRepository* bookRepository,
    std::filesystem::path libraryRoot)
    : m_singleFileImporter(singleFileImporter)
    , m_zipImportCoordinator(zipImportCoordinator)
    , m_bookRepository(bookRepository)
    , m_libraryRoot(std::move(libraryRoot))
{
}

bool CLibraryImportFacade::IsZipPath(const std::filesystem::path& path)
{
    return ToLower(path.extension().string()) == ".zip";
}

void CLibraryImportFacade::RollbackImportedBooks(const std::vector<Librova::Domain::SBookId>& importedBookIds) const
{
    if (m_bookRepository == nullptr || m_libraryRoot.empty())
    {
        return;
    }

    for (auto iterator = importedBookIds.rbegin(); iterator != importedBookIds.rend(); ++iterator)
    {
        const auto book = m_bookRepository->GetById(*iterator);
        if (book.has_value())
        {
            if (book->CoverPath.has_value())
            {
                RemoveManagedPathNoThrow(m_libraryRoot, *book->CoverPath);
            }

            if (book->File.HasManagedPath())
            {
                RemoveManagedPathNoThrow(m_libraryRoot, book->File.ManagedPath);
            }
        }

        m_bookRepository->Remove(*iterator);
    }

    if (Librova::Logging::CLogging::IsInitialized())
    {
        Librova::Logging::Warn(
            "Rolled back {} previously imported book(s) after cancellation.",
            importedBookIds.size());
    }
}

namespace {

struct SExpandedImportSources
{
    std::vector<std::filesystem::path> Candidates;
    std::vector<std::string> Warnings;
};

struct SPreparedImportWorkload
{
    std::size_t TotalEntries = 0;
    std::map<std::filesystem::path, std::size_t> PlannedEntriesBySource;
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
                m_totalEntries,
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
                const auto warning =
                    "Directory '" + PathToUtf8(sourcePath) + "' does not contain supported .fb2, .epub, or .zip files.";
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
                    "Unsupported selected source '" + PathToUtf8(sourcePath) + "'. Only .fb2, .epub, and .zip are supported.";
                expanded.Warnings.push_back(warning);
                LogImportSourceIssueIfInitialized(
                    sourcePath,
                    "source-selection",
                    "skipped",
                    "unsupported-source",
                    warning);
                continue;
            }

            const auto sourceKey = PathToUtf8(sourcePath);
            if (seenPaths.insert(sourceKey).second)
            {
                expanded.Candidates.push_back(sourcePath);
            }

            continue;
        }

        const auto warning = "Selected import source '" + PathToUtf8(sourcePath) + "' does not exist.";
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

[[nodiscard]] SPreparedImportWorkload PrepareImportWorkload(
    const std::vector<std::filesystem::path>& candidates,
    const Librova::ZipImporting::CZipImportCoordinator& zipImportCoordinator,
    const Librova::Domain::IProgressSink& progressSink,
    const std::stop_token stopToken)
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
                const auto plannedEntries = zipImportCoordinator.CountPlannedEntries(sourcePath, &progressSink, stopToken);
                workload.PlannedEntriesBySource.emplace(sourcePath, plannedEntries);
                workload.TotalEntries += plannedEntries;
            }
            catch (const std::exception& error)
            {
                workload.PlannedEntriesBySource.emplace(sourcePath, 1);
                ++workload.TotalEntries;
                const auto warning =
                    "Failed to inspect ZIP archive '" + PathToUtf8(sourcePath) + "': " + error.what();
                workload.Warnings.push_back(warning);
                LogImportSourceIssueIfInitialized(
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
    const auto preparedWorkload = PrepareImportWorkload(expandedSources.Candidates, m_zipImportCoordinator, progressSink, stopToken);
    SImportResult result;
    result.Summary.Mode = DetermineImportMode(request, expandedSources.Candidates);
    result.Summary.Warnings = expandedSources.Warnings;
    MergeWarnings(result.Summary.Warnings, preparedWorkload.Warnings);
    result.Summary.TotalEntries = preparedWorkload.TotalEntries;
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
            const auto zipEntryCountIterator = preparedWorkload.PlannedEntriesBySource.find(sourcePath);
            const auto zipEntryCount = zipEntryCountIterator == preparedWorkload.PlannedEntriesBySource.end()
                ? 1U
                : std::max<std::size_t>(zipEntryCountIterator->second, 1);
            CScopedStructuredProgressSink zipProgressSink(progressSink, result.Summary.TotalEntries, processedEntries, zipEntryCount);
            try
            {
                const auto zipResult = m_zipImportCoordinator.Run({
                    .ZipPath = sourcePath,
                    .WorkingDirectory = request.WorkingDirectory,
                    .AllowProbableDuplicates = request.AllowProbableDuplicates,
                    .ForceEpubConversion = request.ForceEpubConversion
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
                    if (entry.IsImported() && entry.SingleFileResult.has_value() && entry.SingleFileResult->ImportedBookId.has_value())
                    {
                        result.ImportedBookIds.push_back(*entry.SingleFileResult->ImportedBookId);
                    }
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
                        result.WasCancelled = true;
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
            }
            catch (const std::exception& error)
            {
                if (result.Summary.Mode != EImportMode::Batch)
                {
                    throw;
                }

                processedEntries += zipEntryCount;
                result.Summary.FailedEntries += zipEntryCount;
                result.Summary.Warnings.push_back(
                    "ZIP archive '" + PathToUtf8(sourcePath) + "' failed: " + error.what());
                LogImportSourceIssueIfInitialized(
                    sourcePath,
                    "zip-import",
                    "failed",
                    "archive-run-failed",
                    error.what());

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
                        "Archive source failed");
                }
            }

            continue;
        }

        CScopedStructuredProgressSink singleProgressSink(progressSink, result.Summary.TotalEntries, processedEntries, 1);
        const auto singleFileResult = m_singleFileImporter.Run({
            .SourcePath = sourcePath,
            .WorkingDirectory = request.WorkingDirectory,
            .Sha256Hex = expandedSources.Candidates.size() == 1 ? request.Sha256Hex : std::nullopt,
            .AllowProbableDuplicates = request.AllowProbableDuplicates,
            .ForceEpubConversion = request.ForceEpubConversion
        }, singleProgressSink, stopToken);

        ++processedEntries;

        if (result.Summary.Mode == EImportMode::SingleFile)
        {
            result.SingleFileResult = singleFileResult;
        }

        if (singleFileResult.IsSuccess())
        {
            ++result.Summary.ImportedEntries;
            if (singleFileResult.ImportedBookId.has_value())
            {
                result.ImportedBookIds.push_back(*singleFileResult.ImportedBookId);
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
            result.WasCancelled = true;
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

        if (result.Summary.Mode == EImportMode::Batch)
        {
            std::string_view stage = "single-file-import";
            std::string_view outcome = "failed";
            std::string_view status = "failed";

            switch (singleFileResult.Status)
            {
            case Librova::Importing::ESingleFileImportStatus::RejectedDuplicate:
                stage = "duplicate-check";
                outcome = "skipped";
                status = "rejected-duplicate";
                break;
            case Librova::Importing::ESingleFileImportStatus::DecisionRequired:
                stage = "duplicate-check";
                outcome = "skipped";
                status = "decision-required";
                break;
            case Librova::Importing::ESingleFileImportStatus::UnsupportedFormat:
                stage = "format-detection";
                outcome = "skipped";
                status = "unsupported-format";
                break;
            case Librova::Importing::ESingleFileImportStatus::Cancelled:
                stage = "single-file-import";
                outcome = "failed";
                status = "cancelled";
                break;
            case Librova::Importing::ESingleFileImportStatus::Failed:
                stage = "single-file-import";
                outcome = "failed";
                status = "failed";
                break;
            case Librova::Importing::ESingleFileImportStatus::Imported:
                break;
            }

            LogImportSourceIssueIfInitialized(
                sourcePath,
                stage,
                outcome,
                status,
                JoinWarningsAndError(singleFileResult.Warnings, singleFileResult.Error));
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

    result.WasCancelled = result.WasCancelled || stopToken.stop_requested() || progressSink.IsCancellationRequested();
    if (result.WasCancelled && !result.ImportedBookIds.empty())
    {
        RollbackImportedBooks(result.ImportedBookIds);
        result.ImportedBookIds.clear();
        result.Summary.ImportedEntries = 0;
    }

    return result;
}

} // namespace Librova::Application
