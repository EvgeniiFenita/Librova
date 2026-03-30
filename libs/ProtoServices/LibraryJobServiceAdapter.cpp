#include "ProtoServices/LibraryJobServiceAdapter.hpp"

#include "ProtoMapping/LibraryCatalogProtoMapper.hpp"
#include "ProtoMapping/ImportJobProtoMapper.hpp"

namespace Librova::ProtoServices {

CLibraryJobServiceAdapter::CLibraryJobServiceAdapter(
    Librova::ApplicationJobs::CImportJobService& importJobService,
    const Librova::Application::CLibraryCatalogFacade& libraryCatalogFacade)
    : m_importJobService(importJobService)
    , m_libraryCatalogFacade(libraryCatalogFacade)
{
}

librova::v1::StartImportResponse CLibraryJobServiceAdapter::StartImport(
    const librova::v1::StartImportRequest& request) const
{
    const auto importRequest = Librova::ProtoMapping::CImportJobProtoMapper::FromProto(request.import());
    return Librova::ProtoMapping::CImportJobProtoMapper::ToProtoStartResponse(m_importJobService.Start(importRequest));
}

librova::v1::ListBooksResponse CLibraryJobServiceAdapter::ListBooks(
    const librova::v1::ListBooksRequest& request) const
{
    const auto bookListRequest = Librova::ProtoMapping::CLibraryCatalogProtoMapper::FromProto(request.query());
    return Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(
        m_libraryCatalogFacade.ListBooks(bookListRequest));
}

librova::v1::GetBookDetailsResponse CLibraryJobServiceAdapter::GetBookDetails(
    const librova::v1::GetBookDetailsRequest& request) const
{
    const auto details = m_libraryCatalogFacade.GetBookDetails(Librova::Domain::SBookId{request.book_id()});
    return Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(
        details.has_value() ? &*details : nullptr);
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
