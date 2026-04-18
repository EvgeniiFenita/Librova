#include "Import/ImportPerfTracker.hpp"

#include <algorithm>
#include <cstdint>
#include <string>

#include "Foundation/Logging.hpp"
#include "Foundation/UnicodeConversion.hpp"

namespace Librova::Importing {

namespace {

[[nodiscard]] std::uint64_t NsToMs(const std::uint64_t ns) noexcept
{
    return ns / 1'000'000ULL;
}

void LogPeriodic(
    const CImportPerfTracker::SStageStats* stages,
    const std::uint64_t jobId,
    const std::uint64_t bookCount,
    const std::uint64_t importedTotal,
    const std::uint64_t duplicateTotal,
    const std::uint64_t failedTotal,
    const std::uint32_t writerQueueDepth,
    const std::chrono::steady_clock::time_point startTime)
{
    const auto elapsed = std::chrono::steady_clock::now() - startTime;
    const double elapsedSec = std::chrono::duration<double>(elapsed).count();
    const double throughput = elapsedSec > 0.0 ? static_cast<double>(bookCount) / elapsedSec : 0.0;

    // Compute per-stage avg_ms and total worker time for bottleneck %
    std::uint64_t stageTotalMs[CImportPerfTracker::kStageCount]{};
    std::uint64_t workerTotalMs = 0;
    for (std::size_t i = 0; i < CImportPerfTracker::kStageCount; ++i)
    {
        const std::uint64_t totalNs = stages[i].TotalNs.load(std::memory_order_relaxed);
        stageTotalMs[i] = NsToMs(totalNs);
        if (i != static_cast<std::size_t>(CImportPerfTracker::EStage::SemaphoreWait))
        {
            workerTotalMs += stageTotalMs[i];
        }
    }

    // Build stage avg string
    std::string stageLine;
    for (std::size_t i = 0; i < CImportPerfTracker::kStageCount; ++i)
    {
        const std::uint64_t count = stages[i].Count.load(std::memory_order_relaxed);
        const std::uint64_t avgMs = count > 0
            ? NsToMs(stages[i].TotalNs.load(std::memory_order_relaxed)) / count
            : 0;
        if (i > 0)
            stageLine += " | ";
        stageLine += std::string(CImportPerfTracker::kStageNames[i]);
        stageLine += "=";
        stageLine += std::to_string(avgMs);
        stageLine += "ms";
    }

    // Build bottleneck % string (worker stages only, excluding sema_wait)
    struct SEntry
    {
        std::size_t Idx = 0;
        std::uint64_t TotalMs = 0;
    };

    std::array<SEntry, CImportPerfTracker::kStageCount> entries{};
    for (std::size_t i = 0; i < CImportPerfTracker::kStageCount; ++i)
    {
        entries[i] = {
            .Idx = i,
            .TotalMs = stageTotalMs[i]
        };
    }
    std::sort(entries.begin(), entries.end(), [](const SEntry& left, const SEntry& right) {
        return left.TotalMs > right.TotalMs;
    });

    std::string bottleneckLine;
    for (const auto& entry : entries)
    {
        if (entry.Idx == static_cast<std::size_t>(CImportPerfTracker::EStage::SemaphoreWait))
            continue;
        const uint32_t pct = workerTotalMs > 0
            ? static_cast<uint32_t>(entry.TotalMs * 100 / workerTotalMs)
            : 0;
        if (!bottleneckLine.empty())
            bottleneckLine += " ";
        bottleneckLine += std::string(CImportPerfTracker::kStageNames[entry.Idx]);
        bottleneckLine += "=";
        bottleneckLine += std::to_string(pct);
        bottleneckLine += "%";
    }

    if (jobId != 0)
    {
        Librova::Logging::Info(
            "[import-perf] job={} books={} throughput={:.1f} bk/s | {} | "
            "bottleneck: {} | writer_queue={} | imported={} dup={} failed={}",
            jobId,
            bookCount,
            throughput,
            stageLine,
            bottleneckLine,
            writerQueueDepth,
            importedTotal,
            duplicateTotal,
            failedTotal);
        return;
    }

    Librova::Logging::Info(
        "[import-perf] books={} throughput={:.1f} bk/s | {} | "
        "bottleneck: {} | writer_queue={} | imported={} dup={} failed={}",
        bookCount,
        throughput,
        stageLine,
        bottleneckLine,
        writerQueueDepth,
        importedTotal,
        duplicateTotal,
        failedTotal);
}

} // namespace

CImportPerfTracker::CImportPerfTracker(const std::uint64_t jobId) noexcept
    : m_lastLogTimeNs(GetNowNs())
    , m_startTime(std::chrono::steady_clock::now())
    , m_jobId(jobId)
{
}

std::int64_t CImportPerfTracker::GetNowNs() noexcept
{
    return static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

void CImportPerfTracker::OnBookProcessed(
    const std::uint64_t importedDelta,
    const std::uint64_t duplicateDelta,
    const std::uint64_t failedDelta) noexcept
{
    m_importedCount.fetch_add(importedDelta, std::memory_order_relaxed);
    m_duplicateCount.fetch_add(duplicateDelta, std::memory_order_relaxed);
    m_failedCount.fetch_add(failedDelta, std::memory_order_relaxed);

    const std::uint64_t bookCount = m_bookCount.fetch_add(1, std::memory_order_relaxed) + 1;

    std::uint64_t lastLogCount = m_lastLogBookCount.load(std::memory_order_relaxed);
    const bool enoughBooks = (bookCount - lastLogCount) >= kLogEveryN;

    const auto nowNs = GetNowNs();
    const std::int64_t lastLogNs = m_lastLogTimeNs.load(std::memory_order_relaxed);
    const bool enoughTime = (nowNs - lastLogNs) >=
        std::chrono::duration_cast<std::chrono::nanoseconds>(kLogEveryT).count();

    if (!enoughBooks && !enoughTime)
    {
        return;
    }

    // Attempt to claim the log slot — only one thread logs per interval
    if (!m_lastLogBookCount.compare_exchange_strong(
            lastLogCount, bookCount,
            std::memory_order_relaxed))
    {
        return;
    }
    m_lastLogTimeNs.store(nowNs, std::memory_order_relaxed);

    const std::uint32_t queueDepth = m_writerQueueDepthSample.load(std::memory_order_relaxed);

    if (queueDepth >= kWriterQueueWarnThreshold &&
        !m_writerQueueWarnedOnce.exchange(true, std::memory_order_relaxed))
    {
        if (m_jobId != 0)
        {
            Librova::Logging::Warn(
                "[import-perf] job={} Writer queue depth {} exceeds threshold {} — DB writes may be bottlenecking import throughput",
                m_jobId,
                queueDepth,
                kWriterQueueWarnThreshold);
        }
        else
        {
            Librova::Logging::Warn(
                "[import-perf] Writer queue depth {} exceeds threshold {} — DB writes may be bottlenecking import throughput",
                queueDepth,
                kWriterQueueWarnThreshold);
        }
    }

    try
    {
        LogPeriodic(
            m_stages.data(),
            m_jobId,
            bookCount,
            m_importedCount.load(std::memory_order_relaxed),
            m_duplicateCount.load(std::memory_order_relaxed),
            m_failedCount.load(std::memory_order_relaxed),
            queueDepth,
            m_startTime);
    }
    catch (...)
    {
        // Logging must never crash the import
    }
}

void CImportPerfTracker::NoteOutlierIfSlow(
    const std::string_view bookPath,
    const std::chrono::milliseconds elapsed) noexcept
{
    if (elapsed.count() < static_cast<std::int64_t>(kOutlierThresholdMs))
    {
        return;
    }

    try
    {
        if (m_jobId != 0)
        {
            Librova::Logging::Warn(
                "[import-perf] job={} Slow book: {}ms for \"{}\"",
                m_jobId,
                elapsed.count(),
                bookPath);
        }
        else
        {
            Librova::Logging::Warn(
                "[import-perf] Slow book: {}ms for \"{}\"",
                elapsed.count(),
                bookPath);
        }
    }
    catch (...)
    {
    }
}

void CImportPerfTracker::LogSummary(
    const std::chrono::steady_clock::duration totalDuration) noexcept
{
    try
    {
        const std::uint64_t bookCount     = m_bookCount.load(std::memory_order_relaxed);
        const std::uint64_t imported      = m_importedCount.load(std::memory_order_relaxed);
        const std::uint64_t duplicates    = m_duplicateCount.load(std::memory_order_relaxed);
        const std::uint64_t failed        = m_failedCount.load(std::memory_order_relaxed);
        const double elapsedSec = std::chrono::duration<double>(totalDuration).count();
        const double throughput = elapsedSec > 0.0
            ? static_cast<double>(bookCount) / elapsedSec : 0.0;
        const std::uint64_t elapsedMin = static_cast<std::uint64_t>(elapsedSec) / 60;
        const std::uint64_t elapsedSecPart = static_cast<std::uint64_t>(elapsedSec) % 60;

        // Build bottleneck ranking (sorted by total time, descending)
        struct SEntry { std::size_t Idx; std::uint64_t TotalMs; };
        std::array<SEntry, kStageCount> entries{};
        for (std::size_t i = 0; i < kStageCount; ++i)
        {
            entries[i] = {i, NsToMs(m_stages[i].TotalNs.load(std::memory_order_relaxed))};
        }
        std::sort(entries.begin(), entries.end(),
            [](const SEntry& a, const SEntry& b) { return a.TotalMs > b.TotalMs; });

        std::uint64_t workerTotalMs = 0;
        for (std::size_t i = 0; i < kStageCount; ++i)
        {
            if (entries[i].Idx != static_cast<std::size_t>(EStage::SemaphoreWait))
                workerTotalMs += entries[i].TotalMs;
        }

        std::string bottleneckLine;
        for (const auto& entry : entries)
        {
            if (entry.Idx == static_cast<std::size_t>(EStage::SemaphoreWait))
                continue;
            const uint32_t pct = workerTotalMs > 0
                ? static_cast<uint32_t>(entry.TotalMs * 100 / workerTotalMs) : 0;
            if (!bottleneckLine.empty())
                bottleneckLine += " ";
            bottleneckLine += std::string(kStageNames[entry.Idx]);
            bottleneckLine += "=";
            bottleneckLine += std::to_string(pct);
            bottleneckLine += "%";
        }

        if (m_jobId != 0)
        {
            Librova::Logging::Info(
                "[import-perf] SUMMARY job={} total={} imported={} dup={} failed={} | "
                "elapsed={}m{}s throughput={:.1f} bk/s | "
                "bottleneck: {}",
                m_jobId,
                bookCount,
                imported,
                duplicates,
                failed,
                elapsedMin,
                elapsedSecPart,
                throughput,
                bottleneckLine);
            return;
        }

        Librova::Logging::Info(
            "[import-perf] SUMMARY total={} imported={} dup={} failed={} | "
            "elapsed={}m{}s throughput={:.1f} bk/s | "
            "bottleneck: {}",
            bookCount,
            imported,
            duplicates,
            failed,
            elapsedMin,
            elapsedSecPart,
            throughput,
            bottleneckLine);
    }
    catch (...)
    {
    }
}

CImportPerfTracker::SStageSnapshot CImportPerfTracker::SnapshotStages() const noexcept
{
    SStageSnapshot snap;
    for (std::size_t i = 0; i < kStageCount; ++i)
    {
        snap.StageNs[i]     = m_stages[i].TotalNs.load(std::memory_order_relaxed);
        snap.StageCounts[i] = m_stages[i].Count.load(std::memory_order_relaxed);
    }
    snap.BookCount = m_bookCount.load(std::memory_order_relaxed);
    snap.Imported  = m_importedCount.load(std::memory_order_relaxed);
    snap.Duplicate = m_duplicateCount.load(std::memory_order_relaxed);
    snap.Failed    = m_failedCount.load(std::memory_order_relaxed);
    return snap;
}

void CImportPerfTracker::LogArchiveSummary(
    const std::filesystem::path& archivePath,
    const SStageSnapshot& before,
    const std::uint64_t jobId) noexcept
{
    try
    {
        // Compute per-archive deltas
        const std::uint64_t books    = m_bookCount.load(std::memory_order_relaxed) - before.BookCount;
        const std::uint64_t imported = m_importedCount.load(std::memory_order_relaxed)  - before.Imported;
        const std::uint64_t dup      = m_duplicateCount.load(std::memory_order_relaxed) - before.Duplicate;
        const std::uint64_t failed   = m_failedCount.load(std::memory_order_relaxed)    - before.Failed;

        if (books == 0)
        {
            return;
        }

        std::uint64_t deltaNs[kStageCount]{};
        std::uint64_t deltaCount[kStageCount]{};
        std::uint64_t workerTotalMs = 0;

        for (std::size_t i = 0; i < kStageCount; ++i)
        {
            deltaNs[i]    = m_stages[i].TotalNs.load(std::memory_order_relaxed) - before.StageNs[i];
            deltaCount[i] = m_stages[i].Count.load(std::memory_order_relaxed)   - before.StageCounts[i];
            if (i != static_cast<std::size_t>(EStage::SemaphoreWait))
            {
                workerTotalMs += NsToMs(deltaNs[i]);
            }
        }

        // avg ms per stage for this archive
        std::string stageLine;
        for (std::size_t i = 0; i < kStageCount; ++i)
        {
            const std::uint64_t avgMs = deltaCount[i] > 0 ? NsToMs(deltaNs[i]) / deltaCount[i] : 0;
            if (i > 0) stageLine += " | ";
            stageLine += std::string(kStageNames[i]);
            stageLine += "=";
            stageLine += std::to_string(avgMs);
            stageLine += "ms";
        }

        // bottleneck %
        struct SEntry { std::size_t Idx; std::uint64_t TotalMs; };
        std::array<SEntry, kStageCount> entries{};
        for (std::size_t i = 0; i < kStageCount; ++i)
            entries[i] = {i, NsToMs(deltaNs[i])};
        std::sort(entries.begin(), entries.end(),
            [](const SEntry& a, const SEntry& b) { return a.TotalMs > b.TotalMs; });

        std::string bottleneckLine;
        for (const auto& e : entries)
        {
            if (e.Idx == static_cast<std::size_t>(EStage::SemaphoreWait)) continue;
            const std::uint32_t pct = workerTotalMs > 0
                ? static_cast<std::uint32_t>(e.TotalMs * 100 / workerTotalMs) : 0;
            if (!bottleneckLine.empty()) bottleneckLine += " ";
            bottleneckLine += std::string(kStageNames[e.Idx]);
            bottleneckLine += "=";
            bottleneckLine += std::to_string(pct);
            bottleneckLine += "%";
        }

        const std::string archiveUtf8 = Librova::Unicode::PathToUtf8(archivePath);

        if (jobId != 0)
        {
            Librova::Logging::Info(
                "[import-perf] archive job={} archive='{}' books={} imported={} dup={} failed={} | {} | bottleneck: {}",
                jobId, archiveUtf8, books, imported, dup, failed, stageLine, bottleneckLine);
        }
        else
        {
            Librova::Logging::Info(
                "[import-perf] archive archive='{}' books={} imported={} dup={} failed={} | {} | bottleneck: {}",
                archiveUtf8, books, imported, dup, failed, stageLine, bottleneckLine);
        }
    }
    catch (...)
    {
    }
}

} // namespace Librova::Importing
