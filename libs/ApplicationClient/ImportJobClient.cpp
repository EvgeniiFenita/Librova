#include "ApplicationClient/ImportJobClient.hpp"

#include <utility>

#include "ProtoMapping/ImportJobProtoMapper.hpp"

namespace LibriFlow::ApplicationClient {

CImportJobClient::CImportJobClient(std::filesystem::path pipePath)
    : m_pipeClient(std::move(pipePath))
{
}

LibriFlow::ApplicationJobs::TImportJobId CImportJobClient::Start(
    const LibriFlow::Application::SImportRequest& request,
    const std::chrono::milliseconds timeout) const
{
    libriflow::v1::StartImportRequest protoRequest;
    *protoRequest.mutable_import() = LibriFlow::ProtoMapping::CImportJobProtoMapper::ToProto(request);

    const auto response = m_pipeClient.Call<libriflow::v1::StartImportRequest, libriflow::v1::StartImportResponse>(
        LibriFlow::PipeTransport::EPipeMethod::StartImport,
        protoRequest,
        timeout);

    return response.job_id();
}

std::optional<LibriFlow::ApplicationJobs::SImportJobSnapshot> CImportJobClient::TryGetSnapshot(
    const LibriFlow::ApplicationJobs::TImportJobId jobId,
    const std::chrono::milliseconds timeout) const
{
    libriflow::v1::GetImportJobSnapshotRequest request;
    request.set_job_id(jobId);

    const auto response = m_pipeClient.Call<
        libriflow::v1::GetImportJobSnapshotRequest,
        libriflow::v1::GetImportJobSnapshotResponse>(
        LibriFlow::PipeTransport::EPipeMethod::GetImportJobSnapshot,
        request,
        timeout);

    if (!response.has_snapshot())
    {
        return std::nullopt;
    }

    return LibriFlow::ProtoMapping::CImportJobProtoMapper::FromProto(response.snapshot());
}

std::optional<LibriFlow::ApplicationJobs::SImportJobResult> CImportJobClient::TryGetResult(
    const LibriFlow::ApplicationJobs::TImportJobId jobId,
    const std::chrono::milliseconds timeout) const
{
    libriflow::v1::GetImportJobResultRequest request;
    request.set_job_id(jobId);

    const auto response = m_pipeClient.Call<
        libriflow::v1::GetImportJobResultRequest,
        libriflow::v1::GetImportJobResultResponse>(
        LibriFlow::PipeTransport::EPipeMethod::GetImportJobResult,
        request,
        timeout);

    if (!response.has_result())
    {
        return std::nullopt;
    }

    return LibriFlow::ProtoMapping::CImportJobProtoMapper::FromProto(response.result());
}

bool CImportJobClient::Cancel(
    const LibriFlow::ApplicationJobs::TImportJobId jobId,
    const std::chrono::milliseconds timeout) const
{
    libriflow::v1::CancelImportJobRequest request;
    request.set_job_id(jobId);

    const auto response = m_pipeClient.Call<
        libriflow::v1::CancelImportJobRequest,
        libriflow::v1::CancelImportJobResponse>(
        LibriFlow::PipeTransport::EPipeMethod::CancelImportJob,
        request,
        timeout);

    return response.accepted();
}

bool CImportJobClient::Wait(
    const LibriFlow::ApplicationJobs::TImportJobId jobId,
    const std::chrono::milliseconds timeout,
    const std::chrono::milliseconds waitTimeout) const
{
    libriflow::v1::WaitImportJobRequest request;
    request.set_job_id(jobId);
    request.set_timeout_ms(static_cast<std::uint32_t>(waitTimeout.count()));

    const auto response = m_pipeClient.Call<
        libriflow::v1::WaitImportJobRequest,
        libriflow::v1::WaitImportJobResponse>(
        LibriFlow::PipeTransport::EPipeMethod::WaitImportJob,
        request,
        timeout);

    return response.completed();
}

bool CImportJobClient::Remove(
    const LibriFlow::ApplicationJobs::TImportJobId jobId,
    const std::chrono::milliseconds timeout) const
{
    libriflow::v1::RemoveImportJobRequest request;
    request.set_job_id(jobId);

    const auto response = m_pipeClient.Call<
        libriflow::v1::RemoveImportJobRequest,
        libriflow::v1::RemoveImportJobResponse>(
        LibriFlow::PipeTransport::EPipeMethod::RemoveImportJob,
        request,
        timeout);

    return response.removed();
}

} // namespace LibriFlow::ApplicationClient
