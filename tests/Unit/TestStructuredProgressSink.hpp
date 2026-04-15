#pragma once

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

#include "Domain/ServiceContracts.hpp"

class CThreadSafeStructuredProgressSink final
    : public Librova::Domain::IProgressSink
    , public Librova::Domain::IStructuredImportProgressSink
{
public:
    struct SSnapshot
    {
        std::size_t TotalEntries = 0;
        std::size_t ProcessedEntries = 0;
        std::size_t ImportedEntries = 0;
        std::size_t FailedEntries = 0;
        std::size_t SkippedEntries = 0;
        int Percent = 0;
        std::string Message;
    };

    void ReportValue(int, std::string_view) override
    {
    }

    [[nodiscard]] bool IsCancellationRequested() const override
    {
        return false;
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
        const std::scoped_lock lock(m_mutex);
        m_snapshots.push_back({
            .TotalEntries = totalEntries,
            .ProcessedEntries = processedEntries,
            .ImportedEntries = importedEntries,
            .FailedEntries = failedEntries,
            .SkippedEntries = skippedEntries,
            .Percent = percent,
            .Message = std::string{message}
        });
        m_cv.notify_all();
    }

    [[nodiscard]] std::vector<SSnapshot> SnapshotCopy() const
    {
        const std::scoped_lock lock(m_mutex);
        return m_snapshots;
    }

    [[nodiscard]] bool WaitForPartialImportedProgress() const
    {
        std::unique_lock lock(m_mutex);
        return m_cv.wait_for(lock, std::chrono::seconds(2), [this] {
            return std::ranges::any_of(m_snapshots, [](const auto& snapshot) {
                return snapshot.ImportedEntries >= 1
                    && snapshot.ProcessedEntries < snapshot.TotalEntries;
            });
        });
    }

private:
    mutable std::mutex m_mutex;
    mutable std::condition_variable m_cv;
    std::vector<SSnapshot> m_snapshots;
};
