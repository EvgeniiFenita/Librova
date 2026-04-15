#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <utility>

namespace Librova::Importing {

// Thread-safe per-stage import performance tracker.
//
// Usage:
//   CImportPerfTracker perf;
//   {
//       auto _ = perf.MeasureStage(CImportPerfTracker::EStage::Parse);
//       // ... do parse work ...
//   }
//   perf.OnBookProcessed(logger);  // may emit periodic log
//   perf.LogSummary(logger, totalDuration);
class CImportPerfTracker
{
public:
    CImportPerfTracker() noexcept;

    enum class EStage : std::size_t
    {
        Parse = 0,
        Hash,
        FindDuplicates,
        Convert,
        Cover,
        PrepareStorage,
        DbWriteWait,
        ZipScan,
        ZipExtract,
        SemaphoreWait,
        kCount
    };

    static constexpr std::size_t kStageCount = static_cast<std::size_t>(EStage::kCount);

    static constexpr std::string_view kStageNames[kStageCount] = {
        "parse", "hash", "dedup", "convert",
        "cover", "storage", "db_wait", "zip_scan",
        "zip_extract", "sema_wait"
    };

    // RAII stage timer — adds elapsed time to the stage accumulator on destruction.
    class CScopedStageTimer
    {
    public:
        CScopedStageTimer(CImportPerfTracker& tracker, EStage stage) noexcept
            : m_tracker(tracker)
            , m_stage(stage)
            , m_start(std::chrono::steady_clock::now())
        {
        }

        CScopedStageTimer(CScopedStageTimer&& other) noexcept
            : m_tracker(other.m_tracker)
            , m_stage(other.m_stage)
            , m_start(other.m_start)
            , m_active(std::exchange(other.m_active, false))
        {
        }

        ~CScopedStageTimer() noexcept
        {
            if (!m_active) return;
            const auto elapsed = std::chrono::steady_clock::now() - m_start;
            const auto ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
            const auto idx = static_cast<std::size_t>(m_stage);
            m_tracker.m_stages[idx].TotalNs.fetch_add(ns, std::memory_order_relaxed);
            m_tracker.m_stages[idx].Count.fetch_add(1, std::memory_order_relaxed);
        }

        CScopedStageTimer(const CScopedStageTimer&) = delete;
        CScopedStageTimer& operator=(const CScopedStageTimer&) = delete;
        CScopedStageTimer& operator=(CScopedStageTimer&&) = delete;

    private:
        CImportPerfTracker& m_tracker;
        EStage              m_stage;
        std::chrono::steady_clock::time_point m_start;
        bool m_active{true};
    };

    [[nodiscard]] CScopedStageTimer MeasureStage(EStage stage) noexcept
    {
        return CScopedStageTimer(*this, stage);
    }

    // Called by a worker after each book (imported, duplicate, or failed).
    // Emits an Info log every kLogEveryN books or kLogEveryT seconds —
    // whichever comes first. Thread-safe.
    void OnBookProcessed(std::uint64_t importedDelta = 0,
                         std::uint64_t duplicateDelta = 0,
                         std::uint64_t failedDelta    = 0) noexcept;

    // Emits a Warn log when a single book takes longer than kOutlierThresholdMs.
    // Thread-safe. Call with the total per-book elapsed time.
    void NoteOutlierIfSlow(std::string_view bookPath,
                           std::chrono::milliseconds elapsed) noexcept;

    // Samples the current writer queue depth for the next periodic log.
    void SetWriterQueueDepth(std::uint32_t depth) noexcept
    {
        m_writerQueueDepthSample.store(depth, std::memory_order_relaxed);
    }

    // Emits a final summary log. Call once after all workers are done.
    void LogSummary(std::chrono::steady_clock::duration totalDuration) noexcept;

    static constexpr std::uint64_t kLogEveryN             = 500;
    static constexpr auto          kLogEveryT              = std::chrono::seconds(30);
    static constexpr std::uint64_t kOutlierThresholdMs     = 5'000;
    static constexpr std::uint32_t kWriterQueueWarnThreshold = 16;

    // Aligned to cache line to avoid false sharing between worker threads.
    struct alignas(64) SStageStats
    {
        std::atomic<std::uint64_t> TotalNs{0};
        std::atomic<std::uint64_t> Count{0};
    };

private:
    [[nodiscard]] static std::int64_t GetNowNs() noexcept;
    std::array<SStageStats, kStageCount> m_stages{};

    std::atomic<std::uint64_t> m_bookCount{0};
    std::atomic<std::uint64_t> m_importedCount{0};
    std::atomic<std::uint64_t> m_duplicateCount{0};
    std::atomic<std::uint64_t> m_failedCount{0};

    std::atomic<std::uint64_t> m_lastLogBookCount{0};
    std::atomic<std::int64_t>  m_lastLogTimeNs;

    std::atomic<std::uint32_t> m_writerQueueDepthSample{0};
    std::atomic<bool>          m_writerQueueWarnedOnce{false};

    std::chrono::steady_clock::time_point m_startTime;
};

} // namespace Librova::Importing
