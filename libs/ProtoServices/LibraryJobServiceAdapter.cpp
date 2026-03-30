#include "ProtoServices/LibraryJobServiceAdapter.hpp"

#include "ProtoMapping/ImportJobProtoMapper.hpp"

namespace LibriFlow::ProtoServices {

CLibraryJobServiceAdapter::CLibraryJobServiceAdapter(LibriFlow::ApplicationJobs::CImportJobService& importJobService)
    : m_importJobService(importJobService)
{
}

libriflow::v1::StartImportResponse CLibraryJobServiceAdapter::StartImport(
    const libriflow::v1::StartImportRequest& request) const
{
    const auto importRequest = LibriFlow::ProtoMapping::CImportJobProtoMapper::FromProto(request.import());
    return LibriFlow::ProtoMapping::CImportJobProtoMapper::ToProtoStartResponse(m_importJobService.Start(importRequest));
}

libriflow::v1::GetImportJobSnapshotResponse CLibraryJobServiceAdapter::GetImportJobSnapshot(
    const libriflow::v1::GetImportJobSnapshotRequest& request) const
{
    const auto snapshot = m_importJobService.TryGetSnapshot(request.job_id());
    return LibriFlow::ProtoMapping::CImportJobProtoMapper::ToProtoSnapshotResponse(
        snapshot.has_value() ? &*snapshot : nullptr);
}

libriflow::v1::GetImportJobResultResponse CLibraryJobServiceAdapter::GetImportJobResult(
    const libriflow::v1::GetImportJobResultRequest& request) const
{
    const auto result = m_importJobService.TryGetResult(request.job_id());
    return LibriFlow::ProtoMapping::CImportJobProtoMapper::ToProtoResultResponse(
        result.has_value() ? &*result : nullptr);
}

libriflow::v1::WaitImportJobResponse CLibraryJobServiceAdapter::WaitImportJob(
    const libriflow::v1::WaitImportJobRequest& request) const
{
    libriflow::v1::WaitImportJobResponse response;
    response.set_completed(m_importJobService.Wait(request.job_id(), std::chrono::milliseconds(request.timeout_ms())));
    return response;
}

libriflow::v1::CancelImportJobResponse CLibraryJobServiceAdapter::CancelImportJob(
    const libriflow::v1::CancelImportJobRequest& request) const
{
    libriflow::v1::CancelImportJobResponse response;
    response.set_accepted(m_importJobService.Cancel(request.job_id()));
    return response;
}

libriflow::v1::RemoveImportJobResponse CLibraryJobServiceAdapter::RemoveImportJob(
    const libriflow::v1::RemoveImportJobRequest& request) const
{
    libriflow::v1::RemoveImportJobResponse response;
    response.set_removed(m_importJobService.Remove(request.job_id()));
    return response;
}

} // namespace LibriFlow::ProtoServices
