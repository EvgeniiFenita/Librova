#include "ProtoServices/LibraryJobServiceAdapter.hpp"

#include <filesystem>
#include <format>
#include <optional>

#include "Domain/DomainError.hpp"
#include "Logging/Logging.hpp"
#include "ProtoMapping/LibraryCatalogProtoMapper.hpp"
#include "ProtoMapping/ImportJobProtoMapper.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace Librova::ProtoServices {
namespace {

[[nodiscard]] Librova::Domain::SDomainError BuildNotFoundError(const std::string& message)
{
    return Librova::Domain::SDomainError{
        .Code = Librova::Domain::EDomainErrorCode::NotFound,
        .Message = message
    };
}

[[nodiscard]] Librova::Domain::SDomainError BuildDomainError(
    const Librova::Domain::EDomainErrorCode code,
    const std::string& message)
{
    return Librova::Domain::SDomainError{
        .Code = code,
        .Message = message
    };
}

[[nodiscard]] Librova::Domain::SDomainError MapCatalogExceptionToError(const std::exception& error)
{
    const std::string message = error.what();
    if (const auto* domainError = dynamic_cast<const Librova::Domain::CDomainException*>(&error); domainError != nullptr)
    {
        return BuildDomainError(domainError->Code(), message);
    }

    if (dynamic_cast<const std::invalid_argument*>(&error) != nullptr)
    {
        return BuildDomainError(Librova::Domain::EDomainErrorCode::Validation, message);
    }

    return BuildDomainError(Librova::Domain::EDomainErrorCode::IntegrityIssue, message);
}

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
    const Librova::Application::CLibraryImportFacade& libraryImportFacade,
    const Librova::Application::CLibraryCatalogFacade& libraryCatalogFacade,
    const Librova::Application::CLibraryExportFacade& libraryExportFacade,
    const Librova::Application::CLibraryTrashFacade& libraryTrashFacade)
    : m_importJobService(importJobService)
    , m_libraryImportFacade(libraryImportFacade)
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
        "ListBooks returned {} item(s) with TotalCount={}. Query='{}' Languages={} Genres={} Offset={} Limit={}.",
        result.Items.size(),
        result.TotalCount,
        bookListRequest.TextUtf8,
        bookListRequest.Languages.size(),
        bookListRequest.GenresUtf8.size(),
        bookListRequest.Offset,
        bookListRequest.Limit);
    return Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(result);
}

librova::v1::GetBookDetailsResponse CLibraryJobServiceAdapter::GetBookDetails(
    const librova::v1::GetBookDetailsRequest& request) const
{
    try
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

        const auto notFoundError = details.has_value()
            ? std::optional<Librova::Domain::SDomainError>{}
            : std::optional<Librova::Domain::SDomainError>{
                BuildNotFoundError(std::format("Book {} was not found.", request.book_id()))};
        return Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(
            details.has_value() ? &*details : nullptr,
            notFoundError.has_value() ? &*notFoundError : nullptr);
    }
    catch (const std::exception& error)
    {
        LogWarnIfInitialized("GetBookDetails failed for book {}. Error='{}'.", request.book_id(), error.what());
        const auto mappedError = MapCatalogExceptionToError(error);
        return Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(
            static_cast<const Librova::Application::SBookDetails*>(nullptr),
            &mappedError);
    }
}

librova::v1::ExportBookResponse CLibraryJobServiceAdapter::ExportBook(
    const librova::v1::ExportBookRequest& request) const
{
    try
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

        const auto notFoundError = exportedPath.has_value()
            ? std::optional<Librova::Domain::SDomainError>{}
            : std::optional<Librova::Domain::SDomainError>{
                BuildNotFoundError(std::format("Book {} was not found for export.", exportRequest.BookId.Value))};

        return Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(
            exportedPath.has_value() ? &*exportedPath : nullptr,
            notFoundError.has_value() ? &*notFoundError : nullptr);
    }
    catch (const std::exception& error)
    {
        LogWarnIfInitialized("ExportBook failed for book {}. Error='{}'.", request.book_id(), error.what());
        const auto mappedError = MapCatalogExceptionToError(error);
        return Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(
            static_cast<const std::filesystem::path*>(nullptr),
            &mappedError);
    }
}

librova::v1::MoveBookToTrashResponse CLibraryJobServiceAdapter::MoveBookToTrash(
    const librova::v1::MoveBookToTrashRequest& request) const
{
    try
    {
        LogInfoIfInitialized(
            "MoveBookToTrash requested for book {}.",
            request.book_id());

        const auto result = m_libraryTrashFacade.MoveBookToTrash(Librova::Domain::SBookId{request.book_id()});

        if (result.has_value())
        {
            LogInfoIfInitialized(
                "MoveBookToTrash completed for book {} with destination {}{}.",
                request.book_id(),
                result->Destination == Librova::Application::ETrashDestination::RecycleBin
                    ? "RecycleBin"
                    : "ManagedTrash",
                result->HasOrphanedFiles ? " (WARNING: files could not be moved — orphaned on disk)" : "");
        }
        else
        {
            LogWarnIfInitialized("MoveBookToTrash requested unknown book {}.", request.book_id());
        }

        const auto notFoundError = result.has_value()
            ? std::optional<Librova::Domain::SDomainError>{}
            : std::optional<Librova::Domain::SDomainError>{
                BuildNotFoundError(std::format("Book {} was not found for deletion.", request.book_id()))};

        return Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(
            result.has_value() ? &*result : nullptr,
            notFoundError.has_value() ? &*notFoundError : nullptr);
    }
    catch (const std::exception& error)
    {
        LogWarnIfInitialized("MoveBookToTrash failed for book {}. Error='{}'.", request.book_id(), error.what());
        const auto mappedError = MapCatalogExceptionToError(error);
        return Librova::ProtoMapping::CLibraryCatalogProtoMapper::ToProtoResponse(
            static_cast<const Librova::Application::STrashedBookResult*>(nullptr),
            &mappedError);
    }
}

librova::v1::GetImportJobSnapshotResponse CLibraryJobServiceAdapter::GetImportJobSnapshot(
    const librova::v1::GetImportJobSnapshotRequest& request) const
{
    const auto snapshot = m_importJobService.TryGetSnapshot(request.job_id());
    if (snapshot.has_value())
    {
        LogInfoIfInitialized(
            "GetImportJobSnapshot returned status {} for job {}.",
            static_cast<int>(snapshot->Status),
            request.job_id());
    }
    else
    {
        LogWarnIfInitialized("GetImportJobSnapshot requested unknown job {}.", request.job_id());
    }

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
    else
    {
        LogWarnIfInitialized("GetImportJobResult requested unknown job {}.", request.job_id());
    }

    return Librova::ProtoMapping::CImportJobProtoMapper::ToProtoResultResponse(
        result.has_value() ? &*result : nullptr);
}

librova::v1::WaitImportJobResponse CLibraryJobServiceAdapter::WaitImportJob(
    const librova::v1::WaitImportJobRequest& request) const
{
    librova::v1::WaitImportJobResponse response;
    response.set_completed(m_importJobService.Wait(request.job_id(), std::chrono::milliseconds(request.timeout_ms())));
    LogInfoIfInitialized(
        "WaitImportJob for job {} completed={}.",
        request.job_id(),
        response.completed());
    return response;
}

librova::v1::ValidateImportSourcesResponse CLibraryJobServiceAdapter::ValidateImportSources(
    const librova::v1::ValidateImportSourcesRequest& request) const
{
    std::vector<std::filesystem::path> sourcePaths;
    sourcePaths.reserve(static_cast<std::size_t>(request.source_paths_size()));
    for (const auto& sourcePath : request.source_paths())
    {
        sourcePaths.emplace_back(Librova::Unicode::PathFromUtf8(sourcePath));
    }

    const auto validation = m_libraryImportFacade.ValidateImportSources(sourcePaths);
    LogInfoIfInitialized(
        "ValidateImportSources: SourceCount={} blocking={}",
        request.source_paths_size(),
        validation.BlockingMessage.value_or("<none>"));
    librova::v1::ValidateImportSourcesResponse response;
    if (validation.BlockingMessage.has_value())
    {
        response.set_blocking_message(*validation.BlockingMessage);
    }

    return response;
}

librova::v1::CancelImportJobResponse CLibraryJobServiceAdapter::CancelImportJob(
    const librova::v1::CancelImportJobRequest& request) const
{
    librova::v1::CancelImportJobResponse response;
    const auto accepted = m_importJobService.Cancel(request.job_id());
    response.set_accepted(accepted);
    if (!accepted)
    {
        *response.mutable_error() = Librova::ProtoMapping::CImportJobProtoMapper::ToProto(
            BuildNotFoundError(std::format("Import job {} was not found for cancellation.", request.job_id())));
        LogWarnIfInitialized("CancelImportJob requested unknown job {}.", request.job_id());
    }
    else
    {
        LogInfoIfInitialized("CancelImportJob for job {} accepted={}.", request.job_id(), response.accepted());
    }

    return response;
}

librova::v1::RemoveImportJobResponse CLibraryJobServiceAdapter::RemoveImportJob(
    const librova::v1::RemoveImportJobRequest& request) const
{
    librova::v1::RemoveImportJobResponse response;
    const auto removed = m_importJobService.Remove(request.job_id());
    response.set_removed(removed);
    if (!removed)
    {
        *response.mutable_error() = Librova::ProtoMapping::CImportJobProtoMapper::ToProto(
            BuildNotFoundError(std::format("Import job {} was not found for removal.", request.job_id())));
        LogWarnIfInitialized("RemoveImportJob requested unknown job {}.", request.job_id());
    }
    else
    {
        LogInfoIfInitialized("RemoveImportJob for job {} removed={}.", request.job_id(), response.removed());
    }

    return response;
}

} // namespace Librova::ProtoServices
