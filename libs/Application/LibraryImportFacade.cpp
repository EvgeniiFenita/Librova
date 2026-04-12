#include "Application/LibraryImportFacade.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>

#include "Application/ImportRollbackService.hpp"
#include "Application/ImportWorkloadPlanner.hpp"
#include "Application/StructuredProgressMapper.hpp"
#include "ImportSourceExpander/ImportDiagnostics.hpp"
#include "ImportSourceExpander/ImportSourceExpander.hpp"
#include "Logging/Logging.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace {

[[nodiscard]] std::string ToLower(std::string value)
{
    std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
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

[[nodiscard]] std::string GetSingleFileLogReason(const Librova::Importing::SSingleFileImportResult& result)
{
    return JoinWarningsAndError(
        result.Warnings,
        result.DiagnosticError.empty() ? result.Error : result.DiagnosticError);
}

[[nodiscard]] Librova::Application::ENoSuccessfulImportReason CombineNoSuccessfulImportReason(
    const Librova::Application::ENoSuccessfulImportReason currentReason,
    const Librova::Application::ENoSuccessfulImportReason nextReason) noexcept
{
    using Librova::Application::ENoSuccessfulImportReason;

    if (nextReason == ENoSuccessfulImportReason::None)
    {
        return currentReason;
    }

    if (currentReason == ENoSuccessfulImportReason::None)
    {
        return nextReason;
    }

    if (currentReason == ENoSuccessfulImportReason::UnsupportedFormat
        || nextReason == ENoSuccessfulImportReason::UnsupportedFormat)
    {
        return ENoSuccessfulImportReason::UnsupportedFormat;
    }

    return ENoSuccessfulImportReason::DuplicateRejected;
}

[[nodiscard]] Librova::Application::ENoSuccessfulImportReason ClassifySingleFileSkipReason(
    const Librova::Importing::ESingleFileImportStatus status) noexcept
{
    using Librova::Application::ENoSuccessfulImportReason;

    switch (status)
    {
    case Librova::Importing::ESingleFileImportStatus::RejectedDuplicate:
        return ENoSuccessfulImportReason::DuplicateRejected;
    case Librova::Importing::ESingleFileImportStatus::UnsupportedFormat:
        return ENoSuccessfulImportReason::UnsupportedFormat;
    case Librova::Importing::ESingleFileImportStatus::Imported:
    case Librova::Importing::ESingleFileImportStatus::Cancelled:
    case Librova::Importing::ESingleFileImportStatus::Failed:
        return ENoSuccessfulImportReason::None;
    }

    return ENoSuccessfulImportReason::UnsupportedFormat;
}

[[nodiscard]] Librova::Application::ENoSuccessfulImportReason ClassifyZipEntrySkipReason(
    const Librova::ZipImporting::SZipEntryImportResult& entry) noexcept
{
    using Librova::Application::ENoSuccessfulImportReason;

    if (entry.SingleFileResult.has_value())
    {
        return ClassifySingleFileSkipReason(entry.SingleFileResult->Status);
    }

    switch (entry.Status)
    {
    case Librova::ZipImporting::EZipEntryImportStatus::UnsupportedEntry:
    case Librova::ZipImporting::EZipEntryImportStatus::NestedArchiveSkipped:
    case Librova::ZipImporting::EZipEntryImportStatus::Skipped:
        return ENoSuccessfulImportReason::UnsupportedFormat;
    case Librova::ZipImporting::EZipEntryImportStatus::Imported:
    case Librova::ZipImporting::EZipEntryImportStatus::Failed:
    case Librova::ZipImporting::EZipEntryImportStatus::Cancelled:
        return ENoSuccessfulImportReason::None;
    }

    return ENoSuccessfulImportReason::UnsupportedFormat;
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

void RemovePathNoThrow(const std::filesystem::path& path) noexcept
{
    if (path.empty())
    {
        return;
    }

    std::error_code errorCode;
    std::filesystem::remove_all(path, errorCode);
}

void CleanupEntryWorkspaceNoThrow(
    const std::filesystem::path& workingDirectory,
    const std::filesystem::path& entryWorkingDirectory) noexcept
{
    if (entryWorkingDirectory.empty())
    {
        return;
    }

    const auto entriesRoot = (workingDirectory / "entries").lexically_normal();
    RemovePathNoThrow(entryWorkingDirectory);
    CleanupEmptyParentsNoThrow(entryWorkingDirectory, entriesRoot);
    RemovePathNoThrow(entriesRoot);
}

void CleanupWorkingDirectoryNoThrow(
    const std::filesystem::path& libraryRoot,
    const std::filesystem::path& workingDirectory) noexcept
{
    if (libraryRoot.empty() || workingDirectory.empty())
    {
        return;
    }

    const auto generatedWorkingDirectory = (libraryRoot / "Temp" / "UiImport").lexically_normal();
    const auto normalizedWorkingDirectory = workingDirectory.lexically_normal();
    if (normalizedWorkingDirectory != generatedWorkingDirectory)
    {
        return;
    }

    RemovePathNoThrow(normalizedWorkingDirectory / "entries");
    RemovePathNoThrow(normalizedWorkingDirectory / "extracted");
    RemovePathNoThrow(normalizedWorkingDirectory);
}

} // namespace

namespace Librova::Application {

CLibraryImportFacade::CLibraryImportFacade(
    const Librova::Importing::ISingleFileImporter& singleFileImporter,
    const Librova::ZipImporting::CZipImportCoordinator& zipImportCoordinator,
    Librova::Domain::IBookRepository& bookRepository,
    SLibraryImportContext libraryContext)
    : m_singleFileImporter(singleFileImporter)
    , m_zipImportCoordinator(zipImportCoordinator)
    , m_workloadPlanner(zipImportCoordinator)
    , m_rollbackService(bookRepository, libraryContext.LibraryRoot)
    , m_libraryRoot(std::move(libraryContext.LibraryRoot))
{
}

bool CLibraryImportFacade::IsZipPath(const std::filesystem::path& path)
{
    return ToLower(path.extension().string()) == ".zip";
}

namespace {

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

    if (candidates.size() == 1
        && Librova::ImportSourceExpander::CImportSourceExpander::IsSupportedStandaloneImportPath(candidates.front())
        && ToLower(candidates.front().extension().string()) == ".zip")
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

} // namespace

SImportSourceValidation CLibraryImportFacade::ValidateImportSources(
    const std::vector<std::filesystem::path>& sourcePaths) const
{
    if (sourcePaths.empty())
    {
        return {};
    }

    bool hasUsableSource = false;
    bool hasMissingSource = false;

    for (const auto& rawSourcePath : sourcePaths)
    {
        const auto sourcePath = rawSourcePath.lexically_normal();
        if (sourcePath.empty())
        {
            continue;
        }

        std::error_code errorCode;
        if (std::filesystem::is_directory(sourcePath, errorCode))
        {
            hasUsableSource = true;
            continue;
        }

        if (std::filesystem::is_regular_file(sourcePath, errorCode))
        {
            if (Librova::ImportSourceExpander::CImportSourceExpander::IsSupportedStandaloneImportPath(sourcePath))
            {
                hasUsableSource = true;
            }

            continue;
        }

        hasMissingSource = true;
    }

    if (hasUsableSource)
    {
        return {};
    }

    if (hasMissingSource)
    {
        return {.BlockingMessage = "A selected source does not exist."};
    }

    return {.BlockingMessage = Librova::ImportSourceExpander::CImportSourceExpander::BuildSupportedImportSourcesMessage()};
}

SImportResult CLibraryImportFacade::Run(
    const SImportRequest& request,
    Librova::Domain::IProgressSink& progressSink,
    const std::stop_token stopToken) const
{
    if (!request.IsValid())
    {
        throw std::invalid_argument("Import request must contain source path and working directory.");
    }

    const auto expandedSources = Librova::ImportSourceExpander::CImportSourceExpander::Expand(request.SourcePaths);
    const auto preparedWorkload = m_workloadPlanner.Prepare(expandedSources.Candidates, progressSink, stopToken);
    SImportResult result;
    result.Summary.Mode = DetermineImportMode(request, expandedSources.Candidates);
    result.Summary.Warnings = expandedSources.Warnings;
    MergeWarnings(result.Summary.Warnings, preparedWorkload.Warnings);
    result.Summary.TotalEntries = preparedWorkload.TotalEntries;
    result.NoSuccessfulImportReason = expandedSources.Candidates.empty()
        ? ENoSuccessfulImportReason::UnsupportedFormat
        : ENoSuccessfulImportReason::None;
    std::size_t processedEntries = 0;

    ReportStructuredProgressIfSupported(
        progressSink,
        result.Summary.TotalEntries,
        0,
        0,
        0,
        0,
        result.Summary.TotalEntries == 0 ? "No supported import entries were found." : "Prepared import workload");

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
                    case Librova::ZipImporting::EZipEntryImportStatus::Skipped:
                    case Librova::ZipImporting::EZipEntryImportStatus::UnsupportedEntry:
                    case Librova::ZipImporting::EZipEntryImportStatus::NestedArchiveSkipped:
                        ++result.Summary.SkippedEntries;
                        result.NoSuccessfulImportReason = CombineNoSuccessfulImportReason(
                            result.NoSuccessfulImportReason,
                            ClassifyZipEntrySkipReason(entry));
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

                ReportStructuredProgressIfSupported(
                    progressSink,
                    result.Summary.TotalEntries,
                    processedEntries,
                    result.Summary.ImportedEntries,
                    result.Summary.FailedEntries,
                    result.Summary.SkippedEntries,
                    "Processed archive source");
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
                    "ZIP archive '" + Librova::Unicode::PathToUtf8(sourcePath) + "' failed: " + error.what());
                Librova::ImportSourceExpander::LogImportSourceIssueIfInitialized(
                    sourcePath,
                    "zip-import",
                    "failed",
                    "archive-run-failed",
                    error.what());

                ReportStructuredProgressIfSupported(
                    progressSink,
                    result.Summary.TotalEntries,
                    processedEntries,
                    result.Summary.ImportedEntries,
                    result.Summary.FailedEntries,
                    result.Summary.SkippedEntries,
                    "Archive source failed");
            }

            continue;
        }

        const bool usePerEntryWorkingDirectory = result.Summary.Mode == EImportMode::Batch;
        const auto entryWorkingDirectory = usePerEntryWorkingDirectory
            ? request.WorkingDirectory / "entries" / std::to_string(processedEntries + 1)
            : request.WorkingDirectory;
        CScopedStructuredProgressSink singleProgressSink(progressSink, result.Summary.TotalEntries, processedEntries, 1);
        const auto singleFileResult = [&]() {
            try
            {
                return m_singleFileImporter.Run({
                    .SourcePath = sourcePath,
                    .WorkingDirectory = entryWorkingDirectory,
                    .Sha256Hex = expandedSources.Candidates.size() == 1 ? request.Sha256Hex : std::nullopt,
                    .AllowProbableDuplicates = request.AllowProbableDuplicates,
                    .ForceEpubConversion = request.ForceEpubConversion
                }, singleProgressSink, stopToken);
            }
            catch (...)
            {
                if (usePerEntryWorkingDirectory)
                {
                    CleanupEntryWorkspaceNoThrow(request.WorkingDirectory, entryWorkingDirectory);
                }

                throw;
            }
        }();
        if (usePerEntryWorkingDirectory)
        {
            CleanupEntryWorkspaceNoThrow(request.WorkingDirectory, entryWorkingDirectory);
        }

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
            ReportStructuredProgressIfSupported(
                progressSink,
                result.Summary.TotalEntries,
                processedEntries,
                result.Summary.ImportedEntries,
                result.Summary.FailedEntries,
                result.Summary.SkippedEntries,
                "Processed source file");
            continue;
        }

        switch (singleFileResult.Status)
        {
        case Librova::Importing::ESingleFileImportStatus::RejectedDuplicate:
        case Librova::Importing::ESingleFileImportStatus::UnsupportedFormat:
            ++result.Summary.SkippedEntries;
            result.NoSuccessfulImportReason = CombineNoSuccessfulImportReason(
                result.NoSuccessfulImportReason,
                ClassifySingleFileSkipReason(singleFileResult.Status));
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

            Librova::ImportSourceExpander::LogImportSourceIssueIfInitialized(
                sourcePath,
                stage,
                outcome,
                status,
                GetSingleFileLogReason(singleFileResult));
        }

        ReportStructuredProgressIfSupported(
            progressSink,
            result.Summary.TotalEntries,
            processedEntries,
            result.Summary.ImportedEntries,
            result.Summary.FailedEntries,
            result.Summary.SkippedEntries,
            "Processed source file");
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
        const auto rollbackResult = m_rollbackService.RollbackImportedBooks(result.ImportedBookIds);
        MergeWarnings(result.Summary.Warnings, rollbackResult.Warnings);
        result.ImportedBookIds = rollbackResult.RemainingBookIds;
        result.Summary.ImportedEntries = result.ImportedBookIds.size();
        result.HasRollbackCleanupResidue = rollbackResult.HasCleanupResidue;
    }

    CleanupWorkingDirectoryNoThrow(m_libraryRoot, request.WorkingDirectory);

    return result;
}

} // namespace Librova::Application
