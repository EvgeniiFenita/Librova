#include "App/ImportRollbackService.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <system_error>
#include <unordered_set>

#include "Foundation/Logging.hpp"
#include "Storage/ManagedPathSafety.hpp"
#include "Storage/ManagedLibraryLayout.hpp"
#include "Foundation/UnicodeConversion.hpp"

namespace {

[[nodiscard]] std::uint64_t NsToMs(const std::uint64_t ns) noexcept
{
    return ns / 1'000'000ULL;
}

enum class ERollbackPhase : std::size_t
{
    Lookup = 0,
    BatchDelete,
    FallbackDelete,
    CoverCleanup,
    BookCleanup,
    DirectoryCleanup,
    kCount
};

static constexpr std::size_t kRollbackPhaseCount = static_cast<std::size_t>(ERollbackPhase::kCount);

static constexpr std::string_view kRollbackPhaseNames[kRollbackPhaseCount] = {
    "lookup",
    "batch_delete",
    "fallback_delete",
    "cover_cleanup",
    "book_cleanup",
    "directory_cleanup"
};

struct SRollbackPerfStats
{
    std::uint64_t PhaseNs[kRollbackPhaseCount]{};
    std::uint64_t ReachableBookCount = 0;
    std::uint64_t UnreachableBookCount = 0;
    std::uint64_t RemovalFailedCount = 0;
    bool BatchDeleteUsed = false;
    bool FallbackDeleteUsed = false;
};

class CScopedRollbackPhaseTimer final
{
public:
    CScopedRollbackPhaseTimer(SRollbackPerfStats& stats, const ERollbackPhase phase) noexcept
        : m_stats(stats)
        , m_phase(static_cast<std::size_t>(phase))
        , m_start(std::chrono::steady_clock::now())
    {
    }

    ~CScopedRollbackPhaseTimer() noexcept
    {
        const auto elapsed = std::chrono::steady_clock::now() - m_start;
        m_stats.PhaseNs[m_phase] += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
    }

    CScopedRollbackPhaseTimer(const CScopedRollbackPhaseTimer&) = delete;
    CScopedRollbackPhaseTimer& operator=(const CScopedRollbackPhaseTimer&) = delete;

private:
    SRollbackPerfStats& m_stats;
    std::size_t m_phase = 0;
    std::chrono::steady_clock::time_point m_start;
};

void LogRollbackPerfSummaryIfInitialized(
    const std::size_t total,
    const SRollbackPerfStats& stats,
    const std::chrono::steady_clock::duration totalDuration) noexcept
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    try
    {
        struct SEntry
        {
            std::size_t Idx = 0;
            std::uint64_t TotalMs = 0;
        };

        std::array<SEntry, kRollbackPhaseCount> entries{};
        std::uint64_t workerTotalMs = 0;
        std::string phaseLine;

        for (std::size_t i = 0; i < kRollbackPhaseCount; ++i)
        {
            const auto totalMs = NsToMs(stats.PhaseNs[i]);
            entries[i] = {.Idx = i, .TotalMs = totalMs};
            workerTotalMs += totalMs;

            if (!phaseLine.empty())
            {
                phaseLine += " | ";
            }

            phaseLine += std::string(kRollbackPhaseNames[i]);
            phaseLine += "=";
            phaseLine += std::to_string(totalMs);
            phaseLine += "ms";
        }

        std::sort(entries.begin(), entries.end(), [](const SEntry& left, const SEntry& right) {
            return left.TotalMs > right.TotalMs;
        });

        std::string bottleneckLine;
        for (const auto& entry : entries)
        {
            const auto pct = workerTotalMs > 0
                ? static_cast<std::uint32_t>(entry.TotalMs * 100 / workerTotalMs)
                : 0U;

            if (!bottleneckLine.empty())
            {
                bottleneckLine += " ";
            }

            bottleneckLine += std::string(kRollbackPhaseNames[entry.Idx]);
            bottleneckLine += "=";
            bottleneckLine += std::to_string(pct);
            bottleneckLine += "%";
        }

        const auto elapsedSec = std::chrono::duration<double>(totalDuration).count();
        const auto elapsedMin = static_cast<std::uint64_t>(elapsedSec) / 60;
        const auto elapsedSecPart = static_cast<std::uint64_t>(elapsedSec) % 60;

        Librova::Logging::Info(
            "[rollback-perf] SUMMARY total={} reachable={} unreachable={} removal_failed={} "
            "batch_delete_used={} fallback_delete_used={} | elapsed={}m{}s | {} | bottleneck: {}",
            total,
            stats.ReachableBookCount,
            stats.UnreachableBookCount,
            stats.RemovalFailedCount,
            stats.BatchDeleteUsed,
            stats.FallbackDeleteUsed,
            elapsedMin,
            elapsedSecPart,
            phaseLine,
            bottleneckLine);
    }
    catch (...)
    {
    }
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

    if (!normalizedManagedPath.is_absolute()
        && !Librova::ManagedPaths::IsSafeRelativeManagedPath(normalizedManagedPath))
    {
        return std::nullopt;
    }

    const std::filesystem::path candidatePath = normalizedManagedPath.is_absolute()
        ? normalizedManagedPath
        : (root / normalizedManagedPath).lexically_normal();

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

void LogRollbackCleanupIssueIfInitialized(
    const std::filesystem::path& managedPath,
    std::string_view reason) noexcept
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    try
    {
        Librova::Logging::Warn(
            "Import cancellation rollback could not clean up managed path '{}'. Reason='{}'.",
            Librova::Unicode::PathToUtf8(managedPath),
            reason);
    }
    catch (...)
    {
    }
}

[[nodiscard]] std::optional<std::string> CleanupManagedPathNoThrow(
    const std::filesystem::path& root,
    const std::filesystem::path& managedPath) noexcept
{
    if (managedPath.empty())
    {
        return std::nullopt;
    }

    const auto normalizedManagedPath = managedPath.lexically_normal();
    const auto candidatePath = normalizedManagedPath.is_absolute()
        ? normalizedManagedPath
        : (root / normalizedManagedPath).lexically_normal();

    try
    {
        if (!std::filesystem::exists(candidatePath))
        {
            return std::nullopt;
        }

        const auto resolvedPath = TryResolveManagedPathWithinRoot(root, normalizedManagedPath);
        if (!resolvedPath.has_value())
        {
            const std::string reason = "Path could not be resolved safely within the library root.";
            LogRollbackCleanupIssueIfInitialized(normalizedManagedPath, reason);
            return "Cancellation rollback left managed artifact '"
                + Librova::Unicode::PathToUtf8(candidatePath)
                + "' on disk because "
                + reason;
        }

        std::error_code errorCode;
        std::filesystem::remove(*resolvedPath, errorCode);
        if (errorCode)
        {
            const std::string reason = "Filesystem remove failed: " + errorCode.message();
            LogRollbackCleanupIssueIfInitialized(*resolvedPath, reason);
            return "Cancellation rollback left managed artifact '"
                + Librova::Unicode::PathToUtf8(*resolvedPath)
                + "' on disk because "
                + reason;
        }

        if (std::filesystem::exists(*resolvedPath))
        {
            const std::string reason = "Filesystem remove reported success but the path still exists.";
            LogRollbackCleanupIssueIfInitialized(*resolvedPath, reason);
            return "Cancellation rollback left managed artifact '"
                + Librova::Unicode::PathToUtf8(*resolvedPath)
                + "' on disk because "
                + reason;
        }
        return std::nullopt;
    }
    catch (const std::exception& error)
    {
        const std::string reason = std::string{"Cleanup threw an exception: "} + error.what();
        LogRollbackCleanupIssueIfInitialized(normalizedManagedPath, reason);
        return "Cancellation rollback left managed artifact '"
            + Librova::Unicode::PathToUtf8(candidatePath)
            + "' on disk because "
            + reason;
    }
    catch (...)
    {
        const std::string reason = "Cleanup threw a non-standard exception.";
        LogRollbackCleanupIssueIfInitialized(normalizedManagedPath, reason);
        return "Cancellation rollback left managed artifact '"
            + Librova::Unicode::PathToUtf8(candidatePath)
            + "' on disk because "
            + reason;
    }
}

void LogRollbackFailureIfInitialized(
    const Librova::Domain::SBookId bookId,
    const std::exception& error)
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    Librova::Logging::Error(
        "Import cancellation rollback failed for book {}. Error='{}'.",
        bookId.Value,
        error.what());
}

[[nodiscard]] std::optional<std::string> CleanupCancelledBookDirectoryNoThrow(
    const std::filesystem::path& libraryRoot,
    const Librova::Domain::SBookId bookId) noexcept
{
    try
    {
        const auto layout = Librova::StoragePlanning::CManagedLibraryLayout::Build(libraryRoot);
        const auto managedBookDirectory =
            Librova::StoragePlanning::CManagedLibraryLayout::GetObjectShardDirectory(libraryRoot, bookId);

        if (!std::filesystem::exists(managedBookDirectory))
        {
            return std::nullopt;
        }

        if (const auto residue = Librova::ManagedPaths::CleanupEmptyDirectoriesUpTo(
                managedBookDirectory,
                layout.ObjectsDirectory);
            residue.has_value())
        {
            return "Cancellation rollback left managed object shard directory '"
                + Librova::Unicode::PathToUtf8(residue->Path)
                + "' on disk because "
                + residue->Reason;
        }

        return std::nullopt;
    }
    catch (const std::exception& error)
    {
        return "Cancellation rollback left managed object shard directory cleanup incomplete for book "
            + std::to_string(bookId.Value)
            + ": "
            + error.what();
    }
}

} // namespace

namespace Librova::Application {

CImportRollbackService::CImportRollbackService(
    Librova::Domain::IBookRepository& bookRepository,
    std::filesystem::path libraryRoot)
    : m_bookRepository(bookRepository)
    , m_libraryRoot(std::move(libraryRoot))
{
}

SRollbackResult CImportRollbackService::RollbackImportedBooks(
    const std::vector<Librova::Domain::SBookId>& importedBookIds,
    TRollbackProgressCallback progressCallback) const
{
    SRollbackResult rollbackResult;
    auto lastProgressReportAt = std::chrono::steady_clock::now();
    const auto rollbackStartedAt = std::chrono::steady_clock::now();
    SRollbackPerfStats perfStats;

    if (importedBookIds.empty())
    {
        return rollbackResult;
    }

    const std::size_t total = importedBookIds.size();

    if (Librova::Logging::CLogging::IsInitialized())
    {
        Librova::Logging::Info(
            "Cancellation rollback started. Rolling back {} imported book(s).", total);
    }

    // Phase 1: collect managed file paths for each book before mass-deletion.
    // We need the paths to clean up files after the DB rows are gone.
    struct SBookPaths
    {
        Librova::Domain::SBookId Id;
        std::optional<std::filesystem::path> CoverPath;
        std::optional<std::filesystem::path> BookFilePath;
        bool HasManagedFile = false;
    };

    std::vector<SBookPaths> bookPaths;
    bookPaths.reserve(total);
    std::vector<Librova::Domain::SBookId> unreachableIds;

    for (auto iterator = importedBookIds.rbegin(); iterator != importedBookIds.rend(); ++iterator)
    {
        std::optional<Librova::Domain::SBook> book;
        try
        {
            {
                const auto lookupTimer = CScopedRollbackPhaseTimer(perfStats, ERollbackPhase::Lookup);
                book = m_bookRepository.GetById(*iterator);
            }
        }
        catch (const std::exception& error)
        {
            unreachableIds.push_back(*iterator);
            ++perfStats.UnreachableBookCount;
            // Message must contain "catalog lookup failed" — tested by integration tests.
            rollbackResult.Warnings.push_back(
                "Cancellation rollback left book "
                + std::to_string(iterator->Value)
                + " in the library because catalog lookup failed: "
                + error.what());
            LogRollbackFailureIfInitialized(*iterator, error);
            continue;
        }

        SBookPaths paths{.Id = *iterator};
        if (book.has_value())
        {
            paths.CoverPath = book->CoverPath;
            paths.HasManagedFile = book->File.HasManagedPath();
            if (paths.HasManagedFile)
            {
                paths.BookFilePath = book->File.ManagedPath;
            }
        }
        ++perfStats.ReachableBookCount;
        bookPaths.push_back(std::move(paths));
    }

    // Phase 2: batch-delete reachable books in one transaction (no per-book FTS5).
    // Track which IDs fail removal — their files must NOT be deleted in Phase 3.
    std::unordered_set<std::int64_t> removalFailedIds;

    std::vector<Librova::Domain::SBookId> reachableIds;
    reachableIds.reserve(bookPaths.size());
    for (const auto& bp : bookPaths)
    {
        reachableIds.push_back(bp.Id);
    }

    if (!reachableIds.empty())
    {
        try
        {
            perfStats.BatchDeleteUsed = true;
            const auto deleteTimer = CScopedRollbackPhaseTimer(perfStats, ERollbackPhase::BatchDelete);
            m_bookRepository.RemoveBatch(reachableIds);
        }
        catch (const std::exception& error)
        {
            // Batch delete failed — fall back to per-book Remove so we salvage
            // as many deletions as possible and collect accurate residue info.
            if (Librova::Logging::CLogging::IsInitialized())
            {
                Librova::Logging::Warn(
                    "Cancellation rollback: batch delete failed ({}), falling back to per-book removal.",
                    error.what());
            }

            for (const auto& bp : bookPaths)
            {
                try
                {
                    perfStats.FallbackDeleteUsed = true;
                    const auto deleteTimer = CScopedRollbackPhaseTimer(perfStats, ERollbackPhase::FallbackDelete);
                    m_bookRepository.Remove(bp.Id);
                }
                catch (const std::exception& removeError)
                {
                    // Message must contain "catalog removal failed" — tested by integration tests.
                    removalFailedIds.insert(bp.Id.Value);
                    ++perfStats.RemovalFailedCount;
                    rollbackResult.Warnings.push_back(
                        "Cancellation rollback left book "
                        + std::to_string(bp.Id.Value)
                        + " in the library because catalog removal failed: "
                        + removeError.what());
                    LogRollbackFailureIfInitialized(bp.Id, removeError);
                }
            }
        }
    }

    // Phase 3: clean up managed files on disk, reporting progress.
    // Skip file cleanup for books whose DB row could not be removed — they are
    // still in the library and their files must remain intact.
    std::size_t rolledBack = 0;
    for (const auto& bp : bookPaths)
    {
        const bool removalFailed = removalFailedIds.contains(bp.Id.Value);
        if (removalFailed)
        {
            rollbackResult.RemainingBookIds.push_back(bp.Id);
        }
        else
        {
            if (bp.CoverPath.has_value())
            {
                const auto cleanupTimer = CScopedRollbackPhaseTimer(perfStats, ERollbackPhase::CoverCleanup);
                if (const auto cleanupWarning = CleanupManagedPathNoThrow(m_libraryRoot, *bp.CoverPath); cleanupWarning.has_value())
                {
                    rollbackResult.HasCleanupResidue = true;
                    rollbackResult.Warnings.push_back(*cleanupWarning);
                }
            }

            if (bp.HasManagedFile && bp.BookFilePath.has_value())
            {
                {
                    const auto cleanupTimer = CScopedRollbackPhaseTimer(perfStats, ERollbackPhase::BookCleanup);
                    if (const auto cleanupWarning = CleanupManagedPathNoThrow(m_libraryRoot, *bp.BookFilePath); cleanupWarning.has_value())
                    {
                        rollbackResult.HasCleanupResidue = true;
                        rollbackResult.Warnings.push_back(*cleanupWarning);
                    }
                }

                {
                    const auto cleanupTimer = CScopedRollbackPhaseTimer(perfStats, ERollbackPhase::DirectoryCleanup);
                    if (const auto cleanupWarning = CleanupCancelledBookDirectoryNoThrow(m_libraryRoot, bp.Id); cleanupWarning.has_value())
                    {
                        rollbackResult.HasCleanupResidue = true;
                        rollbackResult.Warnings.push_back(*cleanupWarning);
                    }
                }
            }
        }

        ++rolledBack;
        if (progressCallback)
        {
            const auto now = std::chrono::steady_clock::now();
            const bool isComplete = rolledBack == bookPaths.size();
            const bool reportDue = rolledBack == 1
                || (now - lastProgressReportAt) >= kProgressReportInterval;

            if (isComplete || reportDue)
            {
                progressCallback(rolledBack, total);
                lastProgressReportAt = now;
            }
        }
    }

    // Propagate unreachable ids as remaining so callers know about the residue.
    for (const auto& id : unreachableIds)
    {
        rollbackResult.RemainingBookIds.push_back(id);
    }

    LogRollbackPerfSummaryIfInitialized(
        total,
        perfStats,
        std::chrono::steady_clock::now() - rollbackStartedAt);

    if (Librova::Logging::CLogging::IsInitialized())
    {
        if (rollbackResult.RemainingBookIds.empty())
        {
            Librova::Logging::Info(
                "Cancellation rollback complete. Rolled back {} book(s).", total);
        }
        else
        {
            Librova::Logging::Warn(
                "Cancellation rollback complete with residue. {} of {} book(s) could not be fully removed.",
                rollbackResult.RemainingBookIds.size(),
                total);
        }

        if (rollbackResult.HasCleanupResidue)
        {
            Librova::Logging::Warn(
                "Cancellation rollback left managed files or directories on disk for at least one imported book.");
        }
    }

    return rollbackResult;
}

} // namespace Librova::Application
