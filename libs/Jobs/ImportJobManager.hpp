#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <unordered_map>

#include "Jobs/ImportJobRunner.hpp"

namespace LibriFlow::Jobs {

using TImportJobId = std::uint64_t;

struct SImportJobHandle
{
    TImportJobId Id = 0;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return Id != 0;
    }
};

class CImportJobManager final
{
public:
    explicit CImportJobManager(const CImportJobRunner& jobRunner);
    ~CImportJobManager();

    [[nodiscard]] SImportJobHandle Start(const LibriFlow::Application::SImportRequest& request);
    [[nodiscard]] std::optional<SJobProgressSnapshot> TryGetSnapshot(TImportJobId jobId) const;
    [[nodiscard]] std::optional<SImportJobResult> TryGetResult(TImportJobId jobId) const;
    [[nodiscard]] bool Cancel(TImportJobId jobId);
    [[nodiscard]] bool Wait(TImportJobId jobId, std::chrono::milliseconds timeout) const;
    [[nodiscard]] bool Remove(TImportJobId jobId);

private:
    struct SJobRecord
    {
        mutable std::mutex Mutex;
        std::condition_variable Condition;
        SJobProgressSnapshot Snapshot;
        std::optional<SImportJobResult> Result;
        std::jthread Worker;
    };

    [[nodiscard]] std::shared_ptr<SJobRecord> TryGetRecord(TImportJobId jobId) const;

    const CImportJobRunner& m_jobRunner;
    mutable std::mutex m_jobsMutex;
    std::unordered_map<TImportJobId, std::shared_ptr<SJobRecord>> m_jobs;
    TImportJobId m_nextJobId = 1;
};

} // namespace LibriFlow::Jobs
