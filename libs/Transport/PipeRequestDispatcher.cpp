#include "Transport/PipeRequestDispatcher.hpp"

#include <chrono>
#include <google/protobuf/message_lite.h>

#include "Foundation/Logging.hpp"

namespace Librova::PipeTransport {

namespace {

[[nodiscard]] const char* PipeMethodLabel(const EPipeMethod method) noexcept
{
    switch (method)
    {
    case EPipeMethod::StartImport:
        return "StartImport";
    case EPipeMethod::ListBooks:
        return "ListBooks";
    case EPipeMethod::GetBookDetails:
        return "GetBookDetails";
    case EPipeMethod::ExportBook:
        return "ExportBook";
    case EPipeMethod::MoveBookToTrash:
        return "MoveBookToTrash";
    case EPipeMethod::GetImportJobSnapshot:
        return "GetImportJobSnapshot";
    case EPipeMethod::GetImportJobResult:
        return "GetImportJobResult";
    case EPipeMethod::WaitImportJob:
        return "WaitImportJob";
    case EPipeMethod::ValidateImportSources:
        return "ValidateImportSources";
    case EPipeMethod::CancelImportJob:
        return "CancelImportJob";
    case EPipeMethod::RemoveImportJob:
        return "RemoveImportJob";
    case EPipeMethod::ListCollections:
        return "ListCollections";
    case EPipeMethod::CreateCollection:
        return "CreateCollection";
    case EPipeMethod::DeleteCollection:
        return "DeleteCollection";
    case EPipeMethod::AddBookToCollection:
        return "AddBookToCollection";
    case EPipeMethod::RemoveBookFromCollection:
        return "RemoveBookFromCollection";
    default:
        return "Unknown";
    }
}

[[nodiscard]] bool LogSuccessfulDispatchAtInfo(
    const EPipeMethod method,
    const std::chrono::milliseconds elapsed) noexcept
{
    if (elapsed >= std::chrono::milliseconds{500})
    {
        return true;
    }

    return method == EPipeMethod::StartImport
        || method == EPipeMethod::ValidateImportSources
        || method == EPipeMethod::ListCollections
        || method == EPipeMethod::CreateCollection
        || method == EPipeMethod::DeleteCollection
        || method == EPipeMethod::AddBookToCollection
        || method == EPipeMethod::RemoveBookFromCollection
        || method == EPipeMethod::GetImportJobResult
        || method == EPipeMethod::CancelImportJob
        || method == EPipeMethod::RemoveImportJob;
}

} // namespace

CPipeRequestDispatcher::CPipeRequestDispatcher(Librova::ProtoServices::CLibraryJobServiceAdapter& adapter)
    : m_adapter(adapter)
{
}

SPipeResponseEnvelope CPipeRequestDispatcher::Dispatch(const SPipeRequestEnvelope& request) const
{
    switch (request.Method)
    {
    case EPipeMethod::StartImport:
        return DispatchTyped<librova::v1::StartImportRequest, librova::v1::StartImportResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.StartImport(typedRequest);
            });
    case EPipeMethod::ListBooks:
        return DispatchTyped<librova::v1::ListBooksRequest, librova::v1::ListBooksResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.ListBooks(typedRequest);
            });
    case EPipeMethod::GetBookDetails:
        return DispatchTyped<librova::v1::GetBookDetailsRequest, librova::v1::GetBookDetailsResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.GetBookDetails(typedRequest);
            });
    case EPipeMethod::ExportBook:
        return DispatchTyped<librova::v1::ExportBookRequest, librova::v1::ExportBookResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.ExportBook(typedRequest);
            });
    case EPipeMethod::MoveBookToTrash:
        return DispatchTyped<librova::v1::MoveBookToTrashRequest, librova::v1::MoveBookToTrashResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.MoveBookToTrash(typedRequest);
            });
    case EPipeMethod::GetImportJobSnapshot:
        return DispatchTyped<librova::v1::GetImportJobSnapshotRequest, librova::v1::GetImportJobSnapshotResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.GetImportJobSnapshot(typedRequest);
            });
    case EPipeMethod::GetImportJobResult:
        return DispatchTyped<librova::v1::GetImportJobResultRequest, librova::v1::GetImportJobResultResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.GetImportJobResult(typedRequest);
            });
    case EPipeMethod::WaitImportJob:
        return DispatchTyped<librova::v1::WaitImportJobRequest, librova::v1::WaitImportJobResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.WaitImportJob(typedRequest);
            });
    case EPipeMethod::ValidateImportSources:
        return DispatchTyped<librova::v1::ValidateImportSourcesRequest, librova::v1::ValidateImportSourcesResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.ValidateImportSources(typedRequest);
            });
    case EPipeMethod::CancelImportJob:
        return DispatchTyped<librova::v1::CancelImportJobRequest, librova::v1::CancelImportJobResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.CancelImportJob(typedRequest);
            });
    case EPipeMethod::RemoveImportJob:
        return DispatchTyped<librova::v1::RemoveImportJobRequest, librova::v1::RemoveImportJobResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.RemoveImportJob(typedRequest);
            });
    case EPipeMethod::ListCollections:
        return DispatchTyped<librova::v1::ListCollectionsRequest, librova::v1::ListCollectionsResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.ListCollections(typedRequest);
            });
    case EPipeMethod::CreateCollection:
        return DispatchTyped<librova::v1::CreateCollectionRequest, librova::v1::CreateCollectionResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.CreateCollection(typedRequest);
            });
    case EPipeMethod::DeleteCollection:
        return DispatchTyped<librova::v1::DeleteCollectionRequest, librova::v1::DeleteCollectionResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.DeleteCollection(typedRequest);
            });
    case EPipeMethod::AddBookToCollection:
        return DispatchTyped<librova::v1::AddBookToCollectionRequest, librova::v1::AddBookToCollectionResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.AddBookToCollection(typedRequest);
            });
    case EPipeMethod::RemoveBookFromCollection:
        return DispatchTyped<librova::v1::RemoveBookFromCollectionRequest, librova::v1::RemoveBookFromCollectionResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.RemoveBookFromCollection(typedRequest);
            });
    default:
        return {
            .RequestId = request.RequestId,
            .Status = EPipeResponseStatus::UnknownMethod,
            .ErrorMessage = "Unsupported pipe method."
        };
    }
}

template <typename TRequest, typename TResponse, typename TCallback>
SPipeResponseEnvelope CPipeRequestDispatcher::DispatchTyped(
    const SPipeRequestEnvelope& request,
    TCallback&& callback) const
{
    const auto startedAt = std::chrono::steady_clock::now();
    TRequest typedRequest;
    if (!typedRequest.ParseFromString(request.Payload))
    {
        if (Librova::Logging::CLogging::IsInitialized())
        {
            Librova::Logging::Warn(
                "IPC dispatch rejected. RequestId={} Method={} Status=InvalidRequest",
                request.RequestId,
                PipeMethodLabel(request.Method));
        }
        return {
            .RequestId = request.RequestId,
            .Status = EPipeResponseStatus::InvalidRequest,
            .ErrorMessage = "Invalid protobuf request payload."
        };
    }

    TResponse typedResponse;
    try
    {
        typedResponse = callback(typedRequest);
    }
    catch (const std::exception& exception)
    {
        if (Librova::Logging::CLogging::IsInitialized())
        {
            Librova::Logging::Error(
                "IPC dispatch failed. RequestId={} Method={} Status=InternalError Reason='{}'",
                request.RequestId,
                PipeMethodLabel(request.Method),
                exception.what());
        }
        return {
            .RequestId = request.RequestId,
            .Status = EPipeResponseStatus::InternalError,
            .ErrorMessage = exception.what()
        };
    }

    std::string payload;
    if (!typedResponse.SerializeToString(&payload))
    {
        if (Librova::Logging::CLogging::IsInitialized())
        {
            Librova::Logging::Error(
                "IPC dispatch failed. RequestId={} Method={} Status=InternalError Reason='response-serialization-failed'",
                request.RequestId,
                PipeMethodLabel(request.Method));
        }
        return {
            .RequestId = request.RequestId,
            .Status = EPipeResponseStatus::InternalError,
            .ErrorMessage = "Failed to serialize protobuf response payload."
        };
    }

    if (Librova::Logging::CLogging::IsInitialized())
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startedAt);
        if (LogSuccessfulDispatchAtInfo(request.Method, elapsed))
        {
            Librova::Logging::Info(
                "IPC dispatch completed. RequestId={} Method={} Status=Ok ElapsedMs={}",
                request.RequestId,
                PipeMethodLabel(request.Method),
                elapsed.count());
        }
        else
        {
            Librova::Logging::Debug(
                "IPC dispatch completed. RequestId={} Method={} Status=Ok ElapsedMs={}",
                request.RequestId,
                PipeMethodLabel(request.Method),
                elapsed.count());
        }
    }

    return {
        .RequestId = request.RequestId,
        .Status = EPipeResponseStatus::Ok,
        .Payload = std::move(payload)
    };
}

} // namespace Librova::PipeTransport
