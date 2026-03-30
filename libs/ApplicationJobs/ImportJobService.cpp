#include "ApplicationJobs/ImportJobService.hpp"

namespace LibriFlow::ApplicationJobs {

CImportJobService::CImportJobService(LibriFlow::Jobs::CImportJobManager& jobManager)
    : m_jobManager(jobManager)
{
}

TImportJobId CImportJobService::Start(const LibriFlow::Application::SImportRequest& request) const
{
    return m_jobManager.Start(request).Id;
}

std::optional<SImportJobSnapshot> CImportJobService::TryGetSnapshot(const TImportJobId jobId) const
{
    const auto snapshot = m_jobManager.TryGetSnapshot(jobId);
    return snapshot.has_value() ? std::optional<SImportJobSnapshot>{MapSnapshot(jobId, *snapshot)} : std::nullopt;
}

std::optional<SImportJobResult> CImportJobService::TryGetResult(const TImportJobId jobId) const
{
    const auto result = m_jobManager.TryGetResult(jobId);
    return result.has_value() ? std::optional<SImportJobResult>{MapResult(jobId, *result)} : std::nullopt;
}

bool CImportJobService::Cancel(const TImportJobId jobId) const
{
    return m_jobManager.Cancel(jobId);
}

bool CImportJobService::Wait(const TImportJobId jobId, const std::chrono::milliseconds timeout) const
{
    return m_jobManager.Wait(jobId, timeout);
}

EImportJobStatus CImportJobService::MapStatus(const LibriFlow::Jobs::EJobStatus status) noexcept
{
    switch (status)
    {
    case LibriFlow::Jobs::EJobStatus::Pending:
        return EImportJobStatus::Pending;
    case LibriFlow::Jobs::EJobStatus::Running:
        return EImportJobStatus::Running;
    case LibriFlow::Jobs::EJobStatus::Completed:
        return EImportJobStatus::Completed;
    case LibriFlow::Jobs::EJobStatus::Failed:
        return EImportJobStatus::Failed;
    case LibriFlow::Jobs::EJobStatus::Cancelled:
        return EImportJobStatus::Cancelled;
    }

    return EImportJobStatus::Failed;
}

SImportJobSnapshot CImportJobService::MapSnapshot(
    const TImportJobId jobId,
    const LibriFlow::Jobs::SJobProgressSnapshot& snapshot)
{
    return {
        .JobId = jobId,
        .Status = MapStatus(snapshot.Status),
        .Percent = snapshot.Percent,
        .Message = snapshot.Message,
        .Warnings = snapshot.Warnings
    };
}

SImportJobResult CImportJobService::MapResult(
    const TImportJobId jobId,
    const LibriFlow::Jobs::SImportJobResult& result)
{
    return {
        .Snapshot = MapSnapshot(jobId, result.Snapshot),
        .ImportResult = result.ImportResult,
        .Error = result.Error
    };
}

} // namespace LibriFlow::ApplicationJobs
