#include "Application/LibraryImportFacade.hpp"

#include <algorithm>
#include <condition_variable>
#include <cctype>
#include <cmath>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <semaphore>
#include <stdexcept>
#include <thread>

#include <BS_thread_pool.hpp>

#include "Application/ImportRollbackService.hpp"
#include "Application/ImportWorkloadPlanner.hpp"
#include "Application/StructuredProgressMapper.hpp"
#include "ImportSourceExpander/ImportDiagnostics.hpp"
#include "ImportSourceExpander/ImportSourceExpander.hpp"
#include "Importing/ImportDiagnosticText.hpp"
#include "Importing/ImportPerfTracker.hpp"
#include "Importing/ParallelImportHelpers.hpp"
#include "Importing/WriterDispatchingRepository.hpp"
#include "Foundation/Logging.hpp"
#include "Foundation/UnicodeConversion.hpp"

namespace {

[[nodiscard]] std::string ToLower(std::string value)
{
    std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

[[nodiscard]] std::string GetSingleFileLogReason(const Librova::Importing::SSingleFileImportResult& result)
{
    return Librova::Importing::CImportDiagnosticText::JoinWarningsAndError(
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

void CleanupWorkingDirectoryNoThrow(const std::filesystem::path& workingDirectory) noexcept
{
    if (workingDirectory.empty())
    {
        return;
    }

    static constexpr std::string_view kGeneratedRuntimeWorkspaceDirectoryName = "GeneratedUiImportWorkspace";
    static constexpr std::string_view kGeneratedRuntimeWorkspaceParentDirectoryName = "ImportWorkspaces";
    const auto normalizedWorkingDirectory = workingDirectory.lexically_normal();
    const bool isGeneratedRuntimeWorkingDirectory =
        normalizedWorkingDirectory.filename() == kGeneratedRuntimeWorkspaceDirectoryName
        && normalizedWorkingDirectory.has_parent_path()
        && normalizedWorkingDirectory.parent_path().filename() == kGeneratedRuntimeWorkspaceParentDirectoryName;

    if (!isGeneratedRuntimeWorkingDirectory)
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
    , m_bookRepository(bookRepository)
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

struct SParallelEntryResult
{
    std::filesystem::path SourcePath;
    Librova::Importing::SSingleFileImportResult SingleFileResult;
    bool ExceptionThrown = false;
    std::string ExceptionMessage;
};

struct SParallelProgressEvent
{
    std::size_t ImportedDelta = 0;
    std::size_t FailedDelta = 0;
    std::size_t SkippedDelta = 0;
    bool WasCancelled = false;
};

SParallelProgressEvent BuildSingleFileProgressEvent(
    const Librova::Importing::SSingleFileImportResult& singleFileResult) noexcept
{
    using Librova::Importing::ESingleFileImportStatus;

    SParallelProgressEvent event;
    switch (singleFileResult.Status)
    {
    case ESingleFileImportStatus::Imported:
        event.ImportedDelta = 1;
        break;
    case ESingleFileImportStatus::RejectedDuplicate:
    case ESingleFileImportStatus::UnsupportedFormat:
        event.SkippedDelta = 1;
        break;
    case ESingleFileImportStatus::Cancelled:
        event.WasCancelled = true;
        event.SkippedDelta = 1;
        break;
    case ESingleFileImportStatus::Failed:
        event.FailedDelta = 1;
        break;
    }

    return event;
}

void NoteBookProcessed(
    Librova::Importing::CImportPerfTracker& perf,
    const Librova::Importing::SSingleFileImportResult& singleFileResult) noexcept
{
    const bool imported = singleFileResult.IsSuccess();
    const bool cancelled = singleFileResult.Status == Librova::Importing::ESingleFileImportStatus::Cancelled;
    const bool duplicate = singleFileResult.Status == Librova::Importing::ESingleFileImportStatus::RejectedDuplicate;

    perf.OnBookProcessed(
        imported ? 1U : 0U,
        duplicate ? 1U : 0U,
        (!imported && !cancelled && !duplicate) ? 1U : 0U);
}

void NoteBookProcessedForException(Librova::Importing::CImportPerfTracker& perf) noexcept
{
    perf.OnBookProcessed(0, 0, 1);
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
    Librova::Importing::CImportPerfTracker perf(request.JobId);
    const auto runStart = std::chrono::steady_clock::now();
    bool hadFatalWriterError = false;

    ReportStructuredProgressIfSupported(
        progressSink,
        result.Summary.TotalEntries,
        0,
        0,
        0,
        0,
        result.Summary.TotalEntries == 0 ? "No supported import entries were found." : "Prepared import workload");

    // Non-ZIP candidates are collected here so they can be processed in parallel
    // for the whole import without reordering ZIP and non-ZIP sources.
    Librova::Importing::CWriterDispatchingRepository writerRepo(m_bookRepository, &perf);

    auto processSingleFileResultDetails = [&](const std::filesystem::path& sourcePath,
                                              const Librova::Importing::SSingleFileImportResult& singleFileResult) {
        auto logBatchSingleFileIssue = [&]() {
            if (result.Summary.Mode != EImportMode::Batch)
            {
                return;
            }

            std::string_view stage   = "single-file-import";
            std::string_view outcome = "failed";
            std::string_view status  = "failed";

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
                status = "cancelled";
                break;
            case Librova::Importing::ESingleFileImportStatus::Imported:
            case Librova::Importing::ESingleFileImportStatus::Failed:
                break;
            }

            Librova::ImportSourceExpander::LogImportSourceIssueIfInitialized(
                sourcePath, stage, outcome, status, GetSingleFileLogReason(singleFileResult));
        };

        if (singleFileResult.IsSuccess())
        {
            if (singleFileResult.ImportedBookId.has_value())
            {
                result.ImportedBookIds.push_back(*singleFileResult.ImportedBookId);
            }
            return;
        }

        MergeWarnings(result.Summary.Warnings, singleFileResult.Warnings);
        if (!singleFileResult.Error.empty())
        {
            result.Summary.Warnings.push_back(singleFileResult.Error);
        }

        switch (singleFileResult.Status)
        {
        case Librova::Importing::ESingleFileImportStatus::RejectedDuplicate:
        case Librova::Importing::ESingleFileImportStatus::UnsupportedFormat:
            result.NoSuccessfulImportReason = CombineNoSuccessfulImportReason(
                result.NoSuccessfulImportReason,
                ClassifySingleFileSkipReason(singleFileResult.Status));
            logBatchSingleFileIssue();
            break;
        case Librova::Importing::ESingleFileImportStatus::Cancelled:
            result.WasCancelled = true;
            [[fallthrough]];
        case Librova::Importing::ESingleFileImportStatus::Failed:
            logBatchSingleFileIssue();
            break;
        case Librova::Importing::ESingleFileImportStatus::Imported:
            break;
        }
    };

    auto ApplyProgressEvent = [&](const SParallelProgressEvent& event) {
        ++processedEntries;
        result.Summary.ImportedEntries += event.ImportedDelta;
        result.Summary.FailedEntries += event.FailedDelta;
        result.Summary.SkippedEntries += event.SkippedDelta;
        result.WasCancelled = result.WasCancelled || event.WasCancelled;

        ReportStructuredProgressIfSupported(
            progressSink,
            result.Summary.TotalEntries,
            processedEntries,
            result.Summary.ImportedEntries,
            result.Summary.FailedEntries,
            result.Summary.SkippedEntries,
            "Processed source file");
    };

    auto ProcessSequentialSingleSource = [&](const std::filesystem::path& sourcePath) {
        const bool usePerEntryWorkingDirectory = result.Summary.Mode == EImportMode::Batch;
        const auto entryWorkingDirectory = usePerEntryWorkingDirectory
            ? request.WorkingDirectory / "entries" / std::to_string(processedEntries + 1)
            : request.WorkingDirectory;
        CScopedStructuredProgressSink singleProgressSink(progressSink, result.Summary.TotalEntries, processedEntries, 1,
            result.Summary.ImportedEntries, result.Summary.FailedEntries, result.Summary.SkippedEntries);
        const auto singleFileResult = [&]() {
            try
            {
                return m_singleFileImporter.Run({
                    .SourcePath              = sourcePath,
                    .WorkingDirectory        = entryWorkingDirectory,
                    .Sha256Hex               = expandedSources.Candidates.size() == 1 ? request.Sha256Hex : std::nullopt,
                    .ImportJobId            = request.JobId,
                    .AllowProbableDuplicates = request.AllowProbableDuplicates,
                    .ForceEpubConversion     = request.ForceEpubConversion,
                    .PerfTracker             = std::ref(perf),
                    .RepositoryOverride      = result.Summary.Mode == EImportMode::Batch
                        ? std::optional<std::reference_wrapper<Librova::Domain::IBookRepository>>{std::ref(writerRepo)}
                        : std::nullopt,
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

        NoteBookProcessed(perf, singleFileResult);
        ApplyProgressEvent(BuildSingleFileProgressEvent(singleFileResult));

        if (result.Summary.Mode == EImportMode::SingleFile)
        {
            result.SingleFileResult = singleFileResult;
        }

        processSingleFileResultDetails(sourcePath, singleFileResult);
    };

    auto ProcessParallelSingleSources = [&](const std::vector<std::filesystem::path>& sourcePaths) {
        const auto hwThreads  = std::thread::hardware_concurrency();
        const auto threadCount = static_cast<std::size_t>(
            std::max(1u, std::min(8u, hwThreads > 1u ? hwThreads - 1u : 1u)));
        const auto reservedIds = m_bookRepository.ReserveIds(sourcePaths.size());

        std::vector<std::future<SParallelEntryResult>> futures;
        futures.reserve(sourcePaths.size());
        std::size_t submittedCount = 0;
        const std::size_t batchBaseIndex = processedEntries;

        std::mutex progressMutex;
        std::condition_variable progressCv;
        std::deque<SParallelProgressEvent> progressEvents;
        std::size_t completedCount = 0;

        std::counting_semaphore<64> inFlight(static_cast<std::ptrdiff_t>(threadCount) * 2);
        BS::thread_pool<> pool(threadCount);

        auto drainProgressEvents = [&](const bool waitForEvent) {
            std::unique_lock lock(progressMutex);
            if (waitForEvent)
            {
                progressCv.wait(lock, [&] {
                    return !progressEvents.empty() || completedCount == submittedCount;
                });
            }

            while (!progressEvents.empty())
            {
                auto event = std::move(progressEvents.front());
                progressEvents.pop_front();
                ++completedCount;
                // ApplyProgressEvent only mutates run-local state owned by the main thread,
                // so release the queue mutex while workers continue publishing new events.
                lock.unlock();
                ApplyProgressEvent(event);
                lock.lock();
            }
        };

        auto queueProgressEvent = [&](const SParallelProgressEvent& event) {
            {
                const std::scoped_lock lock(progressMutex);
                progressEvents.push_back(event);
            }
            progressCv.notify_one();
        };

        for (std::size_t i = 0; i < sourcePaths.size(); ++i)
        {
            if (stopToken.stop_requested() || progressSink.IsCancellationRequested())
            {
                break;
            }

            const auto candidatePath = sourcePaths[i];
            const auto entryWorkDir  = request.WorkingDirectory / "entries"
                / std::to_string(batchBaseIndex + 1 + i);

            {
                auto semMeasure = perf.MeasureStage(Librova::Importing::CImportPerfTracker::EStage::SemaphoreWait);
                inFlight.acquire();
                (void)semMeasure;
            }

            ++submittedCount;
            futures.push_back(pool.submit_task(
                [this,
                 candidatePath,
                 entryWorkDir,
                 preReservedBookId = reservedIds[i],
                 &request,
                 stopToken,
                 &inFlight,
                 &perf,
                 &writerRepo,
                 &queueProgressEvent]()
                    -> SParallelEntryResult
                {
                    Librova::Importing::SSemRelease<64> guard{inFlight};

                    const auto bookStart = std::chrono::steady_clock::now();
                    Librova::Importing::CNullProgressSink nullSink;
                    SParallelEntryResult entryResult;
                    entryResult.SourcePath = candidatePath;
                    try
                    {
                        entryResult.SingleFileResult = m_singleFileImporter.Run({
                            .SourcePath              = candidatePath,
                            .WorkingDirectory        = entryWorkDir,
                            .PreReservedBookId       = preReservedBookId,
                            .ImportJobId             = request.JobId,
                            .AllowProbableDuplicates = request.AllowProbableDuplicates,
                            .ForceEpubConversion     = request.ForceEpubConversion,
                            .PerfTracker             = std::ref(perf),
                            .RepositoryOverride      = std::ref(writerRepo),
                        }, nullSink, stopToken);
                        RemovePathNoThrow(entryWorkDir);
                        NoteBookProcessed(perf, entryResult.SingleFileResult);
                        queueProgressEvent(BuildSingleFileProgressEvent(entryResult.SingleFileResult));
                    }
                    catch (const std::exception& ex)
                    {
                        RemovePathNoThrow(entryWorkDir);
                        entryResult.ExceptionThrown  = true;
                        entryResult.ExceptionMessage = ex.what();
                        NoteBookProcessedForException(perf);
                        queueProgressEvent({
                            .FailedDelta = 1,
                        });
                    }

                    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - bookStart);
                    perf.NoteOutlierIfSlow(Librova::Unicode::PathToUtf8(candidatePath), elapsed);

                    return entryResult;
                }));
            drainProgressEvents(false);
        }

        while (completedCount < submittedCount)
        {
            drainProgressEvents(true);
        }

        for (auto& future : futures)
        {
            auto parallelResult = future.get();
            if (parallelResult.ExceptionThrown)
            {
                result.Summary.Warnings.push_back(parallelResult.ExceptionMessage);
                Librova::ImportSourceExpander::LogImportSourceIssueIfInitialized(
                    parallelResult.SourcePath, "single-file-import", "failed", "exception",
                    parallelResult.ExceptionMessage);
                continue;
            }

            processSingleFileResultDetails(parallelResult.SourcePath, parallelResult.SingleFileResult);
        }

        const std::size_t unsubmittedCount = sourcePaths.size() - submittedCount;
        if (unsubmittedCount > 0)
        {
            result.WasCancelled = true;
            ReportStructuredProgressIfSupported(
                progressSink,
                result.Summary.TotalEntries,
                processedEntries,
                result.Summary.ImportedEntries,
                result.Summary.FailedEntries,
                result.Summary.SkippedEntries,
                "Import cancelled");
        }

        RemovePathNoThrow(request.WorkingDirectory / "entries");
    };

    for (std::size_t sourceIndex = 0; sourceIndex < expandedSources.Candidates.size();)
    {
        if (hadFatalWriterError || stopToken.stop_requested() || progressSink.IsCancellationRequested())
        {
            break;
        }

        const auto& sourcePath = expandedSources.Candidates[sourceIndex];
        if (IsZipPath(sourcePath))
        {
            const auto zipEntryCountIterator = preparedWorkload.PlannedEntriesBySource.find(sourcePath);
            const auto reservedZipIdCount = zipEntryCountIterator == preparedWorkload.PlannedEntriesBySource.end()
                ? 0U
                : zipEntryCountIterator->second;
            const auto zipEntryCount = zipEntryCountIterator == preparedWorkload.PlannedEntriesBySource.end()
                ? 1U
                : std::max<std::size_t>(zipEntryCountIterator->second, 1);
            CScopedStructuredProgressSink zipProgressSink(progressSink, result.Summary.TotalEntries, processedEntries, zipEntryCount,
                result.Summary.ImportedEntries, result.Summary.FailedEntries, result.Summary.SkippedEntries);
            try
            {
                const auto zipResult = m_zipImportCoordinator.Run({
                    .ZipPath = sourcePath,
                    .WorkingDirectory = request.WorkingDirectory,
                    .JobId = request.JobId,
                    .AllowProbableDuplicates = request.AllowProbableDuplicates,
                    .ForceEpubConversion = request.ForceEpubConversion,
                    .ReservedBookIds = m_bookRepository.ReserveIds(reservedZipIdCount),
                    .WriterRepository = std::ref(writerRepo),
                    .PerfTracker = std::ref(perf),
                }, zipProgressSink, stopToken);

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
                        ++result.Summary.ImportedEntries;
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

            hadFatalWriterError = hadFatalWriterError || writerRepo.HasFatalError();
            ++sourceIndex;
            continue;
        }

        std::vector<std::filesystem::path> singleFileSegment;
        while (sourceIndex < expandedSources.Candidates.size()
            && !IsZipPath(expandedSources.Candidates[sourceIndex]))
        {
            singleFileSegment.push_back(expandedSources.Candidates[sourceIndex]);
            ++sourceIndex;
        }

        const bool useParallelSingleFile = result.Summary.Mode == EImportMode::Batch
            && singleFileSegment.size() > 1
            && !stopToken.stop_requested()
            && !progressSink.IsCancellationRequested();

        if (useParallelSingleFile)
        {
            ProcessParallelSingleSources(singleFileSegment);
            hadFatalWriterError = hadFatalWriterError || writerRepo.HasFatalError();
            continue;
        }

        for (const auto& singleFileSourcePath : singleFileSegment)
        {
            if (hadFatalWriterError || stopToken.stop_requested() || progressSink.IsCancellationRequested())
            {
                break;
            }

            ProcessSequentialSingleSource(singleFileSourcePath);
            hadFatalWriterError = hadFatalWriterError || writerRepo.HasFatalError();
        }
    }

    writerRepo.Drain();
    hadFatalWriterError = hadFatalWriterError || writerRepo.HasFatalError();
    perf.LogSummary(std::chrono::steady_clock::now() - runStart);

    result.WasCancelled= result.WasCancelled || stopToken.stop_requested() || progressSink.IsCancellationRequested();
    if ((result.WasCancelled || hadFatalWriterError) && !result.ImportedBookIds.empty())
    {
        progressSink.BeginRollback(result.ImportedBookIds.size());

        const auto rollbackResult = m_rollbackService.RollbackImportedBooks(
            result.ImportedBookIds,
            [&progressSink](const std::size_t rolledBack, const std::size_t total)
            {
                progressSink.ReportRollbackProgress(rolledBack, total);
            });

        MergeWarnings(result.Summary.Warnings, rollbackResult.Warnings);
        result.ImportedBookIds = rollbackResult.RemainingBookIds;
        result.Summary.ImportedEntries = result.ImportedBookIds.size();
        result.HasRollbackCleanupResidue = rollbackResult.HasCleanupResidue;

        progressSink.BeginCompacting();
        try
        {
            m_bookRepository.Compact([&progressSink]() { progressSink.BeginCompacting(); });
            if (Librova::Logging::CLogging::IsInitialized())
            {
                Librova::Logging::Info("Database compacted after cancellation rollback.");
            }
        }
        catch (const std::exception& compactError)
        {
            if (Librova::Logging::CLogging::IsInitialized())
            {
                Librova::Logging::Warn(
                    "Database compaction after cancellation rollback failed: '{}'.", compactError.what());
            }
        }
    }
    else if (!result.ImportedBookIds.empty())
    {
        try
        {
            m_bookRepository.OptimizeSearchIndex();
        }
        catch (const std::exception& optimizeError)
        {
            if (Librova::Logging::CLogging::IsInitialized())
            {
                Librova::Logging::Warn(
                    "Search-index optimization after import failed: '{}'.",
                    optimizeError.what());
            }
        }
    }

    CleanupWorkingDirectoryNoThrow(request.WorkingDirectory);

    return result;
}

} // namespace Librova::Application
