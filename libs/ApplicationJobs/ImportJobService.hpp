#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Application/LibraryImportFacade.hpp"
#include "Domain/DomainError.hpp"
#include "Jobs/ImportJobManager.hpp"

namespace LibriFlow::ApplicationJobs {

using TImportJobId = std::uint64_t;

enum class EImportJobStatus
{
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled
};

struct SImportJobSnapshot
{
    TImportJobId JobId = 0;
    EImportJobStatus Status = EImportJobStatus::Pending;
    int Percent = 0;
    std::string Message;
    std::vector<std::string> Warnings;

    [[nodiscard]] bool IsTerminal() const noexcept
    {
        return Status == EImportJobStatus::Completed
            || Status == EImportJobStatus::Failed
            || Status == EImportJobStatus::Cancelled;
    }
};

struct SImportJobResult
{
    SImportJobSnapshot Snapshot;
    std::optional<LibriFlow::Application::SImportResult> ImportResult;
    std::optional<LibriFlow::Domain::SDomainError> Error;
};

class CImportJobService final
{
public:
    explicit CImportJobService(LibriFlow::Jobs::CImportJobManager& jobManager);

    [[nodiscard]] TImportJobId Start(const LibriFlow::Application::SImportRequest& request) const;
    [[nodiscard]] std::optional<SImportJobSnapshot> TryGetSnapshot(TImportJobId jobId) const;
    [[nodiscard]] std::optional<SImportJobResult> TryGetResult(TImportJobId jobId) const;
    [[nodiscard]] bool Cancel(TImportJobId jobId) const;
    [[nodiscard]] bool Wait(TImportJobId jobId, std::chrono::milliseconds timeout) const;
    [[nodiscard]] bool Remove(TImportJobId jobId) const;

private:
    [[nodiscard]] static EImportJobStatus MapStatus(LibriFlow::Jobs::EJobStatus status) noexcept;
    [[nodiscard]] static SImportJobSnapshot MapSnapshot(
        TImportJobId jobId,
        const LibriFlow::Jobs::SJobProgressSnapshot& snapshot);
    [[nodiscard]] static SImportJobResult MapResult(
        TImportJobId jobId,
        const LibriFlow::Jobs::SImportJobResult& result);

    LibriFlow::Jobs::CImportJobManager& m_jobManager;
};

} // namespace LibriFlow::ApplicationJobs
