#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Application/LibraryImportFacade.hpp"
#include "Domain/DomainError.hpp"
#include "Jobs/ImportJobManager.hpp"

namespace Librova::ApplicationJobs {

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
    std::optional<Librova::Application::SImportResult> ImportResult;
    std::optional<Librova::Domain::SDomainError> Error;
};

class CImportJobService final
{
public:
    explicit CImportJobService(Librova::Jobs::CImportJobManager& jobManager);

    [[nodiscard]] TImportJobId Start(const Librova::Application::SImportRequest& request) const;
    [[nodiscard]] std::optional<SImportJobSnapshot> TryGetSnapshot(TImportJobId jobId) const;
    [[nodiscard]] std::optional<SImportJobResult> TryGetResult(TImportJobId jobId) const;
    [[nodiscard]] bool Cancel(TImportJobId jobId) const;
    [[nodiscard]] bool Wait(TImportJobId jobId, std::chrono::milliseconds timeout) const;
    [[nodiscard]] bool Remove(TImportJobId jobId) const;

private:
    [[nodiscard]] static EImportJobStatus MapStatus(Librova::Jobs::EJobStatus status) noexcept;
    [[nodiscard]] static SImportJobSnapshot MapSnapshot(
        TImportJobId jobId,
        const Librova::Jobs::SJobProgressSnapshot& snapshot);
    [[nodiscard]] static SImportJobResult MapResult(
        TImportJobId jobId,
        const Librova::Jobs::SImportJobResult& result);

    Librova::Jobs::CImportJobManager& m_jobManager;
};

} // namespace Librova::ApplicationJobs
