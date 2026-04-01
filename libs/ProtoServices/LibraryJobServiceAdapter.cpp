#include "ProtoServices/LibraryJobServiceAdapter.hpp"

#include "Logging/Logging.hpp"
#include "ProtoMapping/LibraryCatalogProtoMapper.hpp"
#include "ProtoMapping/ImportJobProtoMapper.hpp"

namespace Librova::ProtoServices {
namespace {

template <typename... TArgs>
void LogInfoIfInitialized(spdlog::format_string_t<TArgs...> format, TArgs&&... args)
{
    if (Librova::Logging::CLogging::IsInitialized())
    {
        Librova::Logging::Info(format, std::forward<TArgs>(args)...);
    }
}

template <typename... TArgs>
void LogWarnIfInitialized(spdlog::format_string_t<TArgs...> format, TArgs&&... args)
{
    if (Librova::Logging::CLogging::IsInitialized())
    {
        Librova::Logging::Warn(format, std::forward<TArgs>(args)...);
    }
}

} // namespace

CLibraryJobServiceAdapter::CLibraryJobServiceAdapter(
    Librova::ApplicationJobs::CImportJobService& importJobService,
    const Librova::Application::CLibraryCatalogFacade& libraryCatalogFacade,
    const Librova::Application::CLibraryExportFacade& libraryExportFacade,
    const Librova::Application::CLibraryTrashFacade& libraryTrashFacade)
    : m_importJobService(importJobService)
    , m_libraryCatalogFacade(libraryCatalogFacade)
    , m_libraryExportFacade(libraryExportFacade)
    , m_libraryTrashFacade(libraryTrashFacade)
{
}

librova::v1::StartImportResponse CLibraryJobServiceAdapter::StartImport(
    const librova::v1::StartImportRequest& request) const
{
    LogInfoIfInitialized(
        "StartImport requested. SourceCount={} WorkingDirectory='{}' AllowProbableDuplicates={}.",
        request.import().source_paths_size(),
        request.import().working_directory(),
        request.import().allow_probable_duplicates());

    const auto importRequest = Librova::ProtoMapping::CImportJobProtoMapper::FromProto(request.import());
    const auto jobId = m_importJobService.Start(importRequest);
    LogInfoIfInitialized("StartImport queued job {}.", jobId);
    return Librova::ProtoMapping::CImportJobProtoMapper::ToProtoStartResponse(jobId);
}

librova::v1::ListBooksResponse CLibraryJobServiceAdapter::ListBooks(
    const librova::v1::ListBooksRequest& request) const
{
    const auto bookListRequest = Librova::ProtoMapping::CLibraryCatalogProtoMapper::FromProto(request.query());
    const auto result = m_libraryCatalogFacade.ListBooks(bookListRequest);
    LogInfoIfInitialized(
        "ListBooks returned {} item(s). Query='{}' Language='{}' Offset={} Limit={}.",
        result.Items.size(),
        bookListRequest.TextUtf8,
        bookListRequest.Language.value_or(""),
        bookListRequest.Offset,
        bookListRequest.Limit);
    return Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(result);
}

librova::v1::GetBookDetailsResponse CLibraryJobServiceAdapter::GetBookDetails(
    const librova::v1::GetBookDetailsRequest& request) const
{
    const auto details = m_libraryCatalogFacade.GetBookDetails(Librova::Domain::SBookId{request.book_id()});
    if (details.has_value())
    {
        LogInfoIfInitialized("GetBookDetails returned details for book {}.", request.book_id());
    }
    else
    {
        LogWarnIfInitialized("GetBookDetails requested unknown book {}.", request.book_id());
    }
    return Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(
        details.has_value() ? &*details : nullptr);
}

librova::v1::ExportBookResponse CLibraryJobServiceAdapter::ExportBook(
    const librova::v1::ExportBookRequest& request) const
{
    LogInfoIfInitialized(
        "ExportBook requested for book {} to '{}' with format {}.",
        request.book_id(),
        request.destination_path(),
        static_cast<int>(request.export_format()));

    const auto exportRequest = Librova::ProtoMapping::CLibraryCatalogProtoMapper::FromProto(request);
    const auto exportedPath = m_libraryExportFacade.ExportBook(exportRequest);

    if (exportedPath.has_value())
    {
        LogInfoIfInitialized(
            "ExportBook completed for book {} to '{}'.",
            exportRequest.BookId.Value,
            exportedPath->string());
    }
    else
    {
        LogWarnIfInitialized("ExportBook requested unknown book {}.", exportRequest.BookId.Value);
    }

    return Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(
        exportedPath.has_value() ? &*exportedPath : nullptr);
}

librova::v1::MoveBookToTrashResponse CLibraryJobServiceAdapter::MoveBookToTrash(
    const librova::v1::MoveBookToTrashRequest& request) const
{
    LogInfoIfInitialized("MoveBookToTrash requested for book {}.", request.book_id());

    const auto result = m_libraryTrashFacade.MoveBookToTrash(Librova::Domain::SBookId{request.book_id()});

    if (result.has_value())
    {
        LogInfoIfInitialized("MoveBookToTrash completed for book {}.", request.book_id());
    }
    else
    {
        LogWarnIfInitialized("MoveBookToTrash requested unknown book {}.", request.book_id());
    }

    return Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(
        result.has_value() ? &*result : nullptr);
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
    if (result.has_value())
    {
        if (result->Error.has_value())
        {
            LogWarnIfInitialized(
                "Import job {} completed with status {} and error '{}'.",
                request.job_id(),
                static_cast<int>(result->Snapshot.Status),
                result->Error->Message);
        }
        else
        {
            LogInfoIfInitialized(
                "Import job {} completed with status {}. Imported={} Failed={} Skipped={}.",
                request.job_id(),
                static_cast<int>(result->Snapshot.Status),
                result->ImportResult.has_value() ? result->ImportResult->Summary.ImportedEntries : 0,
                result->ImportResult.has_value() ? result->ImportResult->Summary.FailedEntries : 0,
                result->ImportResult.has_value() ? result->ImportResult->Summary.SkippedEntries : 0);
        }
    }
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
    LogInfoIfInitialized("CancelImportJob for job {} accepted={}.", request.job_id(), response.accepted());
    return response;
}

librova::v1::RemoveImportJobResponse CLibraryJobServiceAdapter::RemoveImportJob(
    const librova::v1::RemoveImportJobRequest& request) const
{
    librova::v1::RemoveImportJobResponse response;
    response.set_removed(m_importJobService.Remove(request.job_id()));
    LogInfoIfInitialized("RemoveImportJob for job {} removed={}.", request.job_id(), response.removed());
    return response;
}

} // namespace Librova::ProtoServices
