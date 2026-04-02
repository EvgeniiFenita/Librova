#include "PipeTransport/PipeRequestDispatcher.hpp"

#include <google/protobuf/message_lite.h>

namespace Librova::PipeTransport {

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
    case EPipeMethod::GetLibraryStatistics:
        return DispatchTyped<librova::v1::GetLibraryStatisticsRequest, librova::v1::GetLibraryStatisticsResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.GetLibraryStatistics(typedRequest);
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
    TRequest typedRequest;
    if (!typedRequest.ParseFromString(request.Payload))
    {
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
        return {
            .RequestId = request.RequestId,
            .Status = EPipeResponseStatus::InternalError,
            .ErrorMessage = exception.what()
        };
    }

    std::string payload;
    if (!typedResponse.SerializeToString(&payload))
    {
        return {
            .RequestId = request.RequestId,
            .Status = EPipeResponseStatus::InternalError,
            .ErrorMessage = "Failed to serialize protobuf response payload."
        };
    }

    return {
        .RequestId = request.RequestId,
        .Status = EPipeResponseStatus::Ok,
        .Payload = std::move(payload)
    };
}

} // namespace Librova::PipeTransport
