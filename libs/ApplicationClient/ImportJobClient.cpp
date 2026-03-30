#include "ApplicationClient/ImportJobClient.hpp"

#include <utility>

#include "ProtoMapping/ImportJobProtoMapper.hpp"

namespace Librova::ApplicationClient {

CImportJobClient::CImportJobClient(std::filesystem::path pipePath)
    : m_pipeClient(std::move(pipePath))
{
}

Librova::ApplicationJobs::TImportJobId CImportJobClient::Start(
    const Librova::Application::SImportRequest& request,
    const std::chrono::milliseconds timeout) const
{
    librova::v1::StartImportRequest protoRequest;
    *protoRequest.mutable_import() = Librova::ProtoMapping::CImportJobProtoMapper::ToProto(request);

    const auto response = m_pipeClient.Call<librova::v1::StartImportRequest, librova::v1::StartImportResponse>(
        Librova::PipeTransport::EPipeMethod::StartImport,
        protoRequest,
        timeout);

    return response.job_id();
}

std::optional<Librova::ApplicationJobs::SImportJobSnapshot> CImportJobClient::TryGetSnapshot(
    const Librova::ApplicationJobs::TImportJobId jobId,
    const std::chrono::milliseconds timeout) const
{
    librova::v1::GetImportJobSnapshotRequest request;
    request.set_job_id(jobId);

    const auto response = m_pipeClient.Call<
        librova::v1::GetImportJobSnapshotRequest,
        librova::v1::GetImportJobSnapshotResponse>(
        Librova::PipeTransport::EPipeMethod::GetImportJobSnapshot,
        request,
        timeout);

    if (!response.has_snapshot())
    {
        return std::nullopt;
    }

    return Librova::ProtoMapping::CImportJobProtoMapper::FromProto(response.snapshot());
}

std::optional<Librova::ApplicationJobs::SImportJobResult> CImportJobClient::TryGetResult(
    const Librova::ApplicationJobs::TImportJobId jobId,
    const std::chrono::milliseconds timeout) const
{
    librova::v1::GetImportJobResultRequest request;
    request.set_job_id(jobId);

    const auto response = m_pipeClient.Call<
        librova::v1::GetImportJobResultRequest,
        librova::v1::GetImportJobResultResponse>(
        Librova::PipeTransport::EPipeMethod::GetImportJobResult,
        request,
        timeout);

    if (!response.has_result())
    {
        return std::nullopt;
    }

    return Librova::ProtoMapping::CImportJobProtoMapper::FromProto(response.result());
}

bool CImportJobClient::Cancel(
    const Librova::ApplicationJobs::TImportJobId jobId,
    const std::chrono::milliseconds timeout) const
{
    librova::v1::CancelImportJobRequest request;
    request.set_job_id(jobId);

    const auto response = m_pipeClient.Call<
        librova::v1::CancelImportJobRequest,
        librova::v1::CancelImportJobResponse>(
        Librova::PipeTransport::EPipeMethod::CancelImportJob,
        request,
        timeout);

    return response.accepted();
}

bool CImportJobClient::Wait(
    const Librova::ApplicationJobs::TImportJobId jobId,
    const std::chrono::milliseconds timeout,
    const std::chrono::milliseconds waitTimeout) const
{
    librova::v1::WaitImportJobRequest request;
    request.set_job_id(jobId);
    request.set_timeout_ms(static_cast<std::uint32_t>(waitTimeout.count()));

    const auto response = m_pipeClient.Call<
        librova::v1::WaitImportJobRequest,
        librova::v1::WaitImportJobResponse>(
        Librova::PipeTransport::EPipeMethod::WaitImportJob,
        request,
        timeout);

    return response.completed();
}

bool CImportJobClient::Remove(
    const Librova::ApplicationJobs::TImportJobId jobId,
    const std::chrono::milliseconds timeout) const
{
    librova::v1::RemoveImportJobRequest request;
    request.set_job_id(jobId);

    const auto response = m_pipeClient.Call<
        librova::v1::RemoveImportJobRequest,
        librova::v1::RemoveImportJobResponse>(
        Librova::PipeTransport::EPipeMethod::RemoveImportJob,
        request,
        timeout);

    return response.removed();
}

} // namespace Librova::ApplicationClient
