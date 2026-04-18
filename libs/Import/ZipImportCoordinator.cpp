#include "Import/ZipImportCoordinator.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <fstream>
#include <future>
#include <functional>
#include <mutex>
#include <semaphore>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

#include <zip.h>
#include <BS_thread_pool.hpp>

#include "Import/ImportPerfTracker.hpp"
#include "Import/ImportDiagnosticText.hpp"
#include "Import/ParallelImportHelpers.hpp"
#include "Foundation/Logging.hpp"
#include "Foundation/UnicodeConversion.hpp"

namespace {

[[nodiscard]] std::string GetSingleFileLogReason(const Librova::Importing::SSingleFileImportResult& result)
{
    return Librova::Importing::CImportDiagnosticText::JoinWarningsAndError(
        result.Warnings,
        result.DiagnosticError.empty() ? result.Error : result.DiagnosticError);
}

[[nodiscard]] bool IsSkippedSingleFileResult(const Librova::Importing::SSingleFileImportResult& result) noexcept
{
    return result.Status == Librova::Importing::ESingleFileImportStatus::RejectedDuplicate
        || result.Status == Librova::Importing::ESingleFileImportStatus::UnsupportedFormat;
}

[[nodiscard]] std::string_view GetSingleFileStage(const Librova::Importing::SSingleFileImportResult& result) noexcept
{
    return result.Status == Librova::Importing::ESingleFileImportStatus::RejectedDuplicate
        ? "duplicate-check"
        : "single-file-import";
}

[[nodiscard]] std::string_view GetSingleFileStatus(const Librova::Importing::SSingleFileImportResult& result) noexcept
{
    switch (result.Status)
    {
    case Librova::Importing::ESingleFileImportStatus::RejectedDuplicate:
        return "rejected-duplicate";
    case Librova::Importing::ESingleFileImportStatus::UnsupportedFormat:
        return "unsupported-format";
    case Librova::Importing::ESingleFileImportStatus::Cancelled:
        return "cancelled";
    case Librova::Importing::ESingleFileImportStatus::Failed:
        return "failed";
    case Librova::Importing::ESingleFileImportStatus::Imported:
        return "imported";
    }

    return "failed";
}

void LogZipEntryIssueIfInitialized(
    const std::uint64_t jobId,
    const std::filesystem::path& zipPath,
    const std::filesystem::path& entryPath,
    std::string_view stage,
    std::string_view outcome,
    std::string_view status,
    std::string_view reason)
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    const auto utf8ZipPath = Librova::Unicode::PathToUtf8(zipPath);
    const auto utf8EntryPath = Librova::Unicode::PathToUtf8(entryPath);
    if (outcome == "failed")
    {
        if (jobId != 0)
        {
            Librova::Logging::Error(
                "ZIP entry failed: job={} archive='{}' entry='{}' stage='{}' outcome='{}' status='{}' reason='{}'",
                jobId,
                utf8ZipPath,
                utf8EntryPath,
                stage,
                outcome,
                status,
                reason);
        }
        else
        {
            Librova::Logging::Error(
                "ZIP entry failed: archive='{}' entry='{}' stage='{}' outcome='{}' status='{}' reason='{}'",
                utf8ZipPath,
                utf8EntryPath,
                stage,
                outcome,
                status,
                reason);
        }
        return;
    }

    if (jobId != 0)
    {
        Librova::Logging::Info(
            "ZIP entry skipped: job={} archive='{}' entry='{}' stage='{}' outcome='{}' status='{}' reason='{}'",
            jobId,
            utf8ZipPath,
            utf8EntryPath,
            stage,
            outcome,
            status,
            reason);
        return;
    }

    Librova::Logging::Info(
        "ZIP entry skipped: archive='{}' entry='{}' stage='{}' outcome='{}' status='{}' reason='{}'",
        utf8ZipPath,
        utf8EntryPath,
        stage,
        outcome,
        status,
        reason);
}

class CZipArchive final
{
public:
    explicit CZipArchive(const std::filesystem::path& filePath)
    {
        int errorCode = ZIP_ER_OK;
        const std::string utf8Path = Librova::Unicode::PathToUtf8(filePath);
        m_archive = zip_open(utf8Path.c_str(), ZIP_RDONLY, &errorCode);

        if (m_archive == nullptr)
        {
            zip_error_t errorState;
            zip_error_init_with_code(&errorState, errorCode);
            const std::string message = zip_error_strerror(&errorState);
            zip_error_fini(&errorState);
            throw std::runtime_error("Failed to open ZIP archive: " + message);
        }
    }

    ~CZipArchive()
    {
        if (m_archive != nullptr)
        {
            zip_close(m_archive);
        }
    }

    CZipArchive(const CZipArchive&) = delete;
    CZipArchive& operator=(const CZipArchive&) = delete;
    CZipArchive(CZipArchive&&) = delete;
    CZipArchive& operator=(CZipArchive&&) = delete;

    [[nodiscard]] zip_int64_t GetEntryCount() const
    {
        return zip_get_num_entries(m_archive, 0);
    }

    [[nodiscard]] std::string GetEntryName(const zip_uint64_t index) const
    {
        const char* name = zip_get_name(m_archive, index, ZIP_FL_ENC_GUESS);

        if (name == nullptr)
        {
            throw std::runtime_error("Failed to read ZIP entry name.");
        }

        return name;
    }

    [[nodiscard]] std::vector<std::byte> ReadBytes(const zip_uint64_t index) const
    {
        zip_stat_t stat{};

        if (zip_stat_index(m_archive, index, 0, &stat) != 0)
        {
            throw std::runtime_error("Failed to stat ZIP entry.");
        }

        zip_file_t* file = zip_fopen_index(m_archive, index, 0);

        if (file == nullptr)
        {
            throw std::runtime_error("Failed to open ZIP entry.");
        }

        std::vector<std::byte> bytes(static_cast<std::size_t>(stat.size));
        const zip_int64_t readCount = zip_fread(file, bytes.data(), stat.size);
        zip_fclose(file);

        if (readCount < 0 || static_cast<zip_uint64_t>(readCount) != stat.size)
        {
            throw std::runtime_error("Failed to read ZIP entry payload.");
        }

        return bytes;
    }

private:
    zip_t* m_archive = nullptr;
};

static_assert(!std::is_copy_constructible_v<CZipArchive>);
static_assert(!std::is_move_constructible_v<CZipArchive>);

[[nodiscard]] std::string ToLower(std::string value)
{
    std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

[[nodiscard]] bool IsDirectoryEntry(const std::string_view entryPath)
{
    return !entryPath.empty() && entryPath.back() == '/';
}

std::size_t CountPlannedEntries(
    const CZipArchive& archive,
    const Librova::Domain::IProgressSink* const progressSink,
    const std::stop_token stopToken)
{
    std::size_t plannedEntries = 0;
    const zip_uint64_t entryCount = static_cast<zip_uint64_t>(archive.GetEntryCount());

    for (zip_uint64_t index = 0; index < entryCount; ++index)
    {
        if (stopToken.stop_requested() || (progressSink != nullptr && progressSink->IsCancellationRequested()))
        {
            break;
        }

        const std::string entryName = archive.GetEntryName(index);
        if (!IsDirectoryEntry(entryName))
        {
            ++plannedEntries;
        }
    }

    return plannedEntries;
}

[[nodiscard]] bool IsNestedArchive(const std::filesystem::path& entryPath)
{
    return ToLower(entryPath.extension().string()) == ".zip";
}

[[nodiscard]] bool IsSupportedBookEntry(const std::filesystem::path& entryPath)
{
    const std::string extension = ToLower(entryPath.extension().string());
    return extension == ".epub" || extension == ".fb2";
}

[[nodiscard]] bool IsSafeRelativeEntryPath(const std::filesystem::path& entryPath)
{
    if (entryPath.empty() || entryPath.is_absolute() || entryPath.has_root_name() || entryPath.has_root_directory())
    {
        return false;
    }

    for (const auto& part : entryPath)
    {
        if (part == "..")
        {
            return false;
        }
    }

    return true;
}

void EnsureDirectory(const std::filesystem::path& path)
{
    std::error_code errorCode;
    std::filesystem::create_directories(path, errorCode);

    if (errorCode)
    {
        throw std::runtime_error(
            std::string{"Failed to create directory: "} + Librova::Unicode::PathToUtf8(path));
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

void RemoveEmptyDirectoriesUpToRootNoThrow(
    const std::filesystem::path& rootPath,
    std::filesystem::path currentPath) noexcept
{
    if (rootPath.empty() || currentPath.empty())
    {
        return;
    }

    const std::filesystem::path normalizedRootPath = rootPath.lexically_normal();

    std::error_code errorCode;
    while (!currentPath.empty() && currentPath.lexically_normal() != normalizedRootPath.parent_path())
    {
        // Do not remove the root directory itself — it may still be needed by
        // the main thread (e.g. for extracting subsequent ZIP entries).
        if (currentPath.lexically_normal() == normalizedRootPath)
        {
            break;
        }

        std::filesystem::remove(currentPath, errorCode);
        if (errorCode)
        {
            errorCode.clear();
            break;
        }

        currentPath = currentPath.parent_path();
    }
}

void CleanupExtractedEntryNoThrow(
    const std::filesystem::path& workingDirectory,
    const std::filesystem::path& extractedPath) noexcept
{
    if (extractedPath.empty())
    {
        return;
    }

    const std::filesystem::path extractedRoot = workingDirectory / "extracted";
    RemovePathNoThrow(extractedPath);
    RemoveEmptyDirectoriesUpToRootNoThrow(extractedRoot, extractedPath.parent_path());
}

void CleanupEntryWorkspaceNoThrow(
    const std::filesystem::path& workingDirectory,
    const std::filesystem::path& entryWorkingDirectory) noexcept
{
    if (entryWorkingDirectory.empty())
    {
        return;
    }

    const std::filesystem::path entriesRoot = workingDirectory / "entries";
    RemovePathNoThrow(entryWorkingDirectory);
    RemoveEmptyDirectoriesUpToRootNoThrow(entriesRoot, entryWorkingDirectory.parent_path());
    RemovePathNoThrow(entriesRoot);
}

std::filesystem::path ExtractEntryToWorkspace(
    const CZipArchive& archive,
    const zip_uint64_t index,
    const std::string_view entryName,
    const std::filesystem::path& workingDirectory)
{
    const std::filesystem::path relativePath =
        Librova::Unicode::PathFromUtf8(std::string{entryName}).lexically_normal();

    if (!IsSafeRelativeEntryPath(relativePath))
    {
        throw std::runtime_error("ZIP entry path is unsafe.");
    }

    const std::filesystem::path destinationPath = workingDirectory / "extracted" / relativePath;
    EnsureDirectory(destinationPath.parent_path());

    const std::vector<std::byte> bytes = archive.ReadBytes(index);
    std::ofstream output(destinationPath, std::ios::binary);

    if (!output)
    {
        throw std::runtime_error("Failed to create extracted ZIP entry file.");
    }

    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return destinationPath;
}

} // namespace

namespace Librova::ZipImporting {

std::size_t SZipImportResult::ImportedCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(Entries.begin(), Entries.end(), [](const auto& entry) {
        return entry.IsImported();
    }));
}

CZipImportCoordinator::CZipImportCoordinator(const Librova::Importing::ISingleFileImporter& singleFileImporter)
    : m_singleFileImporter(singleFileImporter)
{
}

std::size_t CZipImportCoordinator::CountPlannedEntries(
    const std::filesystem::path& zipPath,
    const Librova::Domain::IProgressSink* progressSink,
    const std::stop_token stopToken) const
{
    CZipArchive archive(zipPath);
    return ::CountPlannedEntries(archive, progressSink, stopToken);
}

SZipImportResult CZipImportCoordinator::Run(
    const SZipImportRequest& request,
    Librova::Domain::IProgressSink& progressSink,
    const std::stop_token stopToken) const
{
    if (!request.IsValid())
    {
        throw std::invalid_argument("ZIP import request must contain archive path and working directory.");
    }

    Librova::Importing::CImportPerfTracker ownedPerf(request.JobId);
    auto& perf = request.PerfTracker.has_value() ? request.PerfTracker->get() : ownedPerf;
    const bool shouldLogSummary = !request.PerfTracker.has_value();

    // Determine worker count: leave one core free for the OS and UI process.
    const auto hwThreads  = std::thread::hardware_concurrency();
    const auto threadCount = static_cast<std::size_t>(
        std::max(1u, std::min(8u, hwThreads > 1u ? hwThreads - 1u : 1u)));

    const auto runStart = std::chrono::steady_clock::now();
    const auto archivePerfSnapshot = perf.SnapshotStages();

    // inFlight and pool MUST be declared after perf so they are destroyed first.
    // Pool dtor blocks until all submitted workers complete; workers hold &perf.
    std::counting_semaphore<64> inFlight(static_cast<std::ptrdiff_t>(threadCount) * 2);
    BS::thread_pool<> pool(threadCount);

    CZipArchive archive(request.ZipPath);
    const zip_uint64_t entryCount = static_cast<zip_uint64_t>(archive.GetEntryCount());

    if (Librova::Logging::CLogging::IsInitialized())
    {
        if (request.JobId != 0)
        {
            Librova::Logging::Info(
                "zip: scan start job={} archive='{}' raw_entry_count={}",
                request.JobId,
                Librova::Unicode::PathToUtf8(request.ZipPath),
                entryCount);
        }
        else
        {
            Librova::Logging::Info(
                "zip: scan start archive='{}' raw_entry_count={}",
                Librova::Unicode::PathToUtf8(request.ZipPath),
                entryCount);
        }
    }

    const std::size_t totalEntries = [&] {
        auto scanMeasure = perf.MeasureStage(Librova::Importing::CImportPerfTracker::EStage::ZipScan);
        return ::CountPlannedEntries(archive, &progressSink, stopToken);
    }();

    if (Librova::Logging::CLogging::IsInitialized())
    {
        if (request.JobId != 0)
        {
            Librova::Logging::Info(
                "zip: scan done job={} archive='{}' planned_entries={} workers={}",
                request.JobId,
                Librova::Unicode::PathToUtf8(request.ZipPath),
                totalEntries,
                threadCount);
        }
        else
        {
            Librova::Logging::Info(
                "zip: scan done archive='{}' planned_entries={} workers={}",
                Librova::Unicode::PathToUtf8(request.ZipPath),
                totalEntries,
                threadCount);
        }
    }
    std::size_t reservedIdIndex = 0;

    // Progress counters and result declared before Phase 1 so eager collection
    // during Phase 1 can report live progress to callers.
    auto* structuredSink = dynamic_cast<Librova::Domain::IStructuredImportProgressSink*>(&progressSink);

    std::size_t processedEntries  = 0;
    std::size_t importedEntries   = 0;
    std::size_t failedEntries     = 0;
    std::size_t skippedEntries    = 0;
    std::size_t cancelledEntries  = 0;

    SZipImportResult result;

    // Phase 1 ── Main thread: iterate ZIP sequentially, extract + submit to pool.
    //
    // Each non-directory entry occupies exactly one slot (preserves ZIP order).
    // Skipped entries carry an immediate result; real entries carry a future.
    //
    // The semaphore limits how many extracted files sit on disk at once.
    // zip_t* is NOT thread-safe: ALL extraction happens on the main thread.

    struct SEntrySlot
    {
        // Exactly one of the two optional members is populated.
        std::optional<SZipEntryImportResult>              Immediate;
        std::optional<std::future<SZipEntryImportResult>> Future;
    };

    std::vector<SEntrySlot> slots;
    slots.reserve(static_cast<std::size_t>(entryCount));

    struct SProgressEvent
    {
        std::size_t ImportedDelta = 0;
        std::size_t FailedDelta = 0;
        std::size_t SkippedDelta = 0;
        std::size_t CancelledDelta = 0;
    };

    std::mutex progressMutex;
    std::condition_variable progressCv;
    std::deque<SProgressEvent> progressEvents;
    std::size_t completedWorkers = 0;
    std::size_t submittedWorkers = 0;

    auto applyProgressEvent = [&](const SProgressEvent& event) {
        ++processedEntries;
        importedEntries += event.ImportedDelta;
        failedEntries += event.FailedDelta;
        skippedEntries += event.SkippedDelta;
        cancelledEntries += event.CancelledDelta;
        if (structuredSink != nullptr)
        {
            const auto percent = static_cast<int>(
                (processedEntries * 100) / std::max<std::size_t>(totalEntries, 1));
            structuredSink->ReportStructuredProgress(
                totalEntries, processedEntries, importedEntries, failedEntries,
                skippedEntries + cancelledEntries, percent, "Processed ZIP entry");
        }
    };

    auto drainProgressEvents = [&](const bool waitForEvent) {
        std::unique_lock lock(progressMutex);
        if (waitForEvent)
        {
            progressCv.wait(lock, [&] {
                return !progressEvents.empty() || completedWorkers == submittedWorkers;
            });
        }

        while (!progressEvents.empty())
        {
            auto event = std::move(progressEvents.front());
            progressEvents.pop_front();
            ++completedWorkers;
            // applyProgressEvent only touches coordinator-local counters on the main thread,
            // so release the queue mutex while workers keep publishing completion events.
            lock.unlock();
            applyProgressEvent(event);
            lock.lock();
        }
    };

    auto QueueProgressEvent = [&](const SProgressEvent& event) {
        {
            const std::scoped_lock lock(progressMutex);
            progressEvents.push_back(event);
        }
        progressCv.notify_one();
    };

    auto BuildEventFromEntryStatus = [](const EZipEntryImportStatus status) {
        SProgressEvent event;
        switch (status)
        {
        case EZipEntryImportStatus::Imported:
            event.ImportedDelta = 1;
            break;
        case EZipEntryImportStatus::Failed:
            event.FailedDelta = 1;
            break;
        case EZipEntryImportStatus::Skipped:
        case EZipEntryImportStatus::UnsupportedEntry:
        case EZipEntryImportStatus::NestedArchiveSkipped:
            event.SkippedDelta = 1;
            break;
        case EZipEntryImportStatus::Cancelled:
            event.CancelledDelta = 1;
            break;
        }
        return event;
    };

    auto AddImmediateEntry = [&](SZipEntryImportResult entryResult) {
        applyProgressEvent(BuildEventFromEntryStatus(entryResult.Status));
        slots.push_back({.Immediate = std::move(entryResult)});
    };

    bool cancelledInPhase1 = false;

    for (zip_uint64_t index = 0; index < entryCount; ++index)
    {
        if (stopToken.stop_requested() || progressSink.IsCancellationRequested())
        {
            cancelledInPhase1 = true;
            break;
        }

        const std::string entryName = archive.GetEntryName(index);
        const std::filesystem::path entryPath = Librova::Unicode::PathFromUtf8(entryName);

        // Directory entries — no slot, no-op
        if (IsDirectoryEntry(entryName))
        {
            continue;
        }

        // --- Immediate skips (no extraction needed) ---

        if (IsNestedArchive(entryPath))
        {
            const std::string err = "Nested ZIP archives are not supported.";
            LogZipEntryIssueIfInitialized(
                request.JobId,
                request.ZipPath, entryPath, "zip-entry-filter",
                "skipped", "nested-archive-skipped", err);
            AddImmediateEntry({
                .ArchivePath = entryPath,
                .Status      = EZipEntryImportStatus::NestedArchiveSkipped,
                .Error       = err
            });
            drainProgressEvents(false);
            continue;
        }

        if (!IsSafeRelativeEntryPath(entryPath.lexically_normal()))
        {
            const std::string err = "Unsafe ZIP entry path.";
            LogZipEntryIssueIfInitialized(
                request.JobId,
                request.ZipPath, entryPath, "zip-entry-filter",
                "skipped", "unsafe-entry-path", err);
            AddImmediateEntry({
                .ArchivePath = entryPath,
                .Status      = EZipEntryImportStatus::UnsupportedEntry,
                .Error       = err
            });
            drainProgressEvents(false);
            continue;
        }

        if (!IsSupportedBookEntry(entryPath))
        {
            const std::string err = "Unsupported ZIP entry format.";
            LogZipEntryIssueIfInitialized(
                request.JobId,
                request.ZipPath, entryPath, "zip-entry-filter",
                "skipped", "unsupported-entry-format", err);
            AddImmediateEntry({
                .ArchivePath = entryPath,
                .Status      = EZipEntryImportStatus::UnsupportedEntry,
                .Error       = err
            });
            drainProgressEvents(false);
            continue;
        }

        // --- Entries that need import ---

        // Acquire semaphore on the main thread BEFORE extraction.
        // This limits concurrent extracted files on disk (≈ threadCount×2 files peak).
        {
            auto semMeasure = perf.MeasureStage(Librova::Importing::CImportPerfTracker::EStage::SemaphoreWait);
            inFlight.acquire();
            (void)semMeasure;
        }

        // Extraction MUST stay on main thread (zip_t* is not thread-safe).
        std::filesystem::path extractedPath;
        try
        {
            auto extractMeasure = perf.MeasureStage(Librova::Importing::CImportPerfTracker::EStage::ZipExtract);
            extractedPath = ExtractEntryToWorkspace(archive, index, entryName, request.WorkingDirectory);
            (void)extractMeasure;
        }
        catch (const std::exception& ex)
        {
            inFlight.release();
            perf.OnBookProcessed(0u, 0u, 1u);
            LogZipEntryIssueIfInitialized(
                request.JobId,
                request.ZipPath, entryPath, "extraction", "failed", "extract-error", ex.what());
            AddImmediateEntry({
                .ArchivePath = entryPath,
                .Status      = EZipEntryImportStatus::Failed,
                .Error       = ex.what()
            });
            drainProgressEvents(false);
            continue;
        }

        // Use index-based working directory to guarantee uniqueness across parallel workers.
        // (Stem-based paths can collide when different subdirectories contain same-named files.)
        const std::filesystem::path entryWorkDir =
            request.WorkingDirectory / "entries" / std::to_string(static_cast<std::uint64_t>(index));

        ++submittedWorkers;
        slots.push_back({.Future = pool.submit_task(
            [this,
             reservedBookId = reservedIdIndex < request.ReservedBookIds.size()
                 ? std::optional<Librova::Domain::SBookId>{request.ReservedBookIds[reservedIdIndex]}
                 : std::nullopt,
             entryPath,
             extractedPath  = std::move(extractedPath),
             entryWorkDir   = std::move(entryWorkDir),
             &request,
             &inFlight,
             &perf,
             &QueueProgressEvent,
             &BuildEventFromEntryStatus,
             stopToken]() -> SZipEntryImportResult
            {
                // RAII: release the semaphore slot when this worker finishes.
                Librova::Importing::SSemRelease<64> semGuard{inFlight};

                const auto entryStart = std::chrono::steady_clock::now();

                Librova::Importing::CNullProgressSink nullSink;
                Librova::Importing::SSingleFileImportResult singleResult;
                try
                {
                    singleResult = m_singleFileImporter.Run({
                        .SourcePath              = extractedPath,
                        .WorkingDirectory        = entryWorkDir,
                        .PreReservedBookId       = reservedBookId,
                        .LogicalSourceLabel      = Librova::Unicode::PathToUtf8(request.ZipPath) + "::" + Librova::Unicode::PathToUtf8(entryPath),
                        .ImportJobId             = request.JobId,
                        .AllowProbableDuplicates = request.AllowProbableDuplicates,
                        .ForceEpubConversion     = request.ForceEpubConversion,
                        .PerfTracker             = std::ref(perf),
                        .RepositoryOverride      = request.WriterRepository,
                    }, nullSink, stopToken);
                }
                catch (const std::exception& ex)
                {
                    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - entryStart);
                    CleanupExtractedEntryNoThrow(request.WorkingDirectory, extractedPath);
                    RemovePathNoThrow(entryWorkDir);
                    LogZipEntryIssueIfInitialized(
                        request.JobId,
                        request.ZipPath, entryPath, "single-file-import",
                        "failed", "exception", ex.what());
                    perf.NoteOutlierIfSlow(Librova::Unicode::PathToUtf8(entryPath), elapsed);
                    perf.OnBookProcessed(0u, 0u, 1u);
                    QueueProgressEvent(BuildEventFromEntryStatus(EZipEntryImportStatus::Failed));
                    return {
                        .ArchivePath = entryPath,
                        .Status      = EZipEntryImportStatus::Failed,
                        .Error       = ex.what()
                    };
                }

                CleanupExtractedEntryNoThrow(request.WorkingDirectory, extractedPath);
                RemovePathNoThrow(entryWorkDir);

                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - entryStart);
                perf.NoteOutlierIfSlow(Librova::Unicode::PathToUtf8(entryPath), elapsed);

                const bool imported = singleResult.IsSuccess();
                const bool cancelled = singleResult.Status ==
                    Librova::Importing::ESingleFileImportStatus::Cancelled;

                perf.OnBookProcessed(
                    imported ? 1u : 0u,
                    singleResult.Status == Librova::Importing::ESingleFileImportStatus::RejectedDuplicate ? 1u : 0u,
                    (!imported && !cancelled && singleResult.Status != Librova::Importing::ESingleFileImportStatus::RejectedDuplicate) ? 1u : 0u);

                if (!imported && !cancelled && !IsSkippedSingleFileResult(singleResult))
                {
                    LogZipEntryIssueIfInitialized(
                        request.JobId,
                        request.ZipPath, entryPath,
                        GetSingleFileStage(singleResult), "failed",
                        GetSingleFileStatus(singleResult),
                        GetSingleFileLogReason(singleResult));
                }
                else if (IsSkippedSingleFileResult(singleResult))
                {
                    LogZipEntryIssueIfInitialized(
                        request.JobId,
                        request.ZipPath, entryPath,
                        GetSingleFileStage(singleResult), "skipped",
                        GetSingleFileStatus(singleResult),
                        GetSingleFileLogReason(singleResult));
                }

                const auto entryStatus = imported
                    ? EZipEntryImportStatus::Imported
                    : (cancelled
                        ? EZipEntryImportStatus::Cancelled
                        : (IsSkippedSingleFileResult(singleResult)
                            ? EZipEntryImportStatus::Skipped
                            : EZipEntryImportStatus::Failed));
                QueueProgressEvent(BuildEventFromEntryStatus(entryStatus));

                return {
                    .ArchivePath      = entryPath,
                    .Status           = entryStatus,
                    .SingleFileResult = std::move(singleResult)
                };
            })
        });
        ++reservedIdIndex;
        drainProgressEvents(false);
    }

    while (completedWorkers < submittedWorkers)
    {
        drainProgressEvents(true);
    }

    result.Entries.reserve(slots.size() + (cancelledInPhase1 ? 1U : 0U));
    for (auto& slot : slots)
    {
        result.Entries.push_back(slot.Immediate.has_value()
            ? std::move(*slot.Immediate)
            : slot.Future->get());
    }

    if (cancelledInPhase1)
    {
        result.Entries.push_back({.Status = EZipEntryImportStatus::Cancelled});
    }

    // All workers have finished (all Phase 2 fut.get() calls returned); safe to clean up.
    RemovePathNoThrow(request.WorkingDirectory / "extracted");
    RemovePathNoThrow(request.WorkingDirectory / "entries");

    if (shouldLogSummary)
    {
        perf.LogSummary(std::chrono::steady_clock::now() - runStart);
    }

    perf.LogArchiveSummary(request.ZipPath, archivePerfSnapshot, request.JobId);

    if (Librova::Logging::CLogging::IsInitialized())
    {
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - runStart).count();
        if (request.JobId != 0)
        {
            Librova::Logging::Info(
                "zip: done job={} archive='{}' imported={} failed={} skipped={} elapsed_ms={}",
                request.JobId,
                Librova::Unicode::PathToUtf8(request.ZipPath),
                importedEntries,
                failedEntries,
                skippedEntries,
                elapsedMs);
        }
        else
        {
            Librova::Logging::Info(
                "zip: done archive='{}' imported={} failed={} skipped={} elapsed_ms={}",
                Librova::Unicode::PathToUtf8(request.ZipPath),
                importedEntries,
                failedEntries,
                skippedEntries,
                elapsedMs);
        }
    }

    return result;
}

} // namespace Librova::ZipImporting
