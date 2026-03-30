#include "ProtoServices/LibraryJobServiceAdapter.hpp"

#include "ProtoMapping/ImportJobProtoMapper.hpp"

namespace Librova::ProtoServices {

CLibraryJobServiceAdapter::CLibraryJobServiceAdapter(Librova::ApplicationJobs::CImportJobService& importJobService)
    : m_importJobService(importJobService)
{
}

librova::v1::StartImportResponse CLibraryJobServiceAdapter::StartImport(
    const librova::v1::StartImportRequest& request) const
{
    const auto importRequest = Librova::ProtoMapping::CImportJobProtoMapper::FromProto(request.import());
    return Librova::ProtoMapping::CImportJobProtoMapper::ToProtoStartResponse(m_importJobService.Start(importRequest));
}

librova::v1::GetImportJobSnapshotResponse CLibraryJobServiceAdapter::GetImportJobSnapshot(
    const librova::v1::GetImportJobSnapshotRequest& request) const
{
    const auto snapshot = m_importJobService.TryGetSnapshot(request.job_id());
    return Librova::ProtoMapping::CImportJobProtoMapper::ToProtoSnapshotResponse(
        snapshot.has_value() ? &*snapshot : nullptr);
}

librova::v1::GetImportJobResultResponse CLibraryJobServiceAdapter::GetImportJobResult(
    const librova::v1::GetImportJobResultRequest& request) const
{
    const auto result = m_importJobService.TryGetResult(request.job_id());
    return Librova::ProtoMapping::CImportJobProtoMapper::ToProtoResultResponse(
        result.has_value() ? &*result : nullptr);
}

librova::v1::WaitImportJobResponse CLibraryJobServiceAdapter::WaitImportJob(
    const librova::v1::WaitImportJobRequest& request) const
{
    librova::v1::WaitImportJobResponse response;
    response.set_completed(m_importJobService.Wait(request.job_id(), std::chrono::milliseconds(request.timeout_ms())));
    return response;
}

librova::v1::CancelImportJobResponse CLibraryJobServiceAdapter::CancelImportJob(
    const librova::v1::CancelImportJobRequest& request) const
{
    librova::v1::CancelImportJobResponse response;
    response.set_accepted(m_importJobService.Cancel(request.job_id()));
    return response;
}

librova::v1::RemoveImportJobResponse CLibraryJobServiceAdapter::RemoveImportJob(
    const librova::v1::RemoveImportJobRequest& request) const
{
    librova::v1::RemoveImportJobResponse response;
    response.set_removed(m_importJobService.Remove(request.job_id()));
    return response;
}

} // namespace Librova::ProtoServices
