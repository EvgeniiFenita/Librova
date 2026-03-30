#include "PipeTransport/PipeRequestDispatcher.hpp"

#include <google/protobuf/message_lite.h>

namespace LibriFlow::PipeTransport {

CPipeRequestDispatcher::CPipeRequestDispatcher(LibriFlow::ProtoServices::CLibraryJobServiceAdapter& adapter)
    : m_adapter(adapter)
{
}

SPipeResponseEnvelope CPipeRequestDispatcher::Dispatch(const SPipeRequestEnvelope& request) const
{
    switch (request.Method)
    {
    case EPipeMethod::StartImport:
        return DispatchTyped<libriflow::v1::StartImportRequest, libriflow::v1::StartImportResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.StartImport(typedRequest);
            });
    case EPipeMethod::GetImportJobSnapshot:
        return DispatchTyped<libriflow::v1::GetImportJobSnapshotRequest, libriflow::v1::GetImportJobSnapshotResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.GetImportJobSnapshot(typedRequest);
            });
    case EPipeMethod::GetImportJobResult:
        return DispatchTyped<libriflow::v1::GetImportJobResultRequest, libriflow::v1::GetImportJobResultResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.GetImportJobResult(typedRequest);
            });
    case EPipeMethod::WaitImportJob:
        return DispatchTyped<libriflow::v1::WaitImportJobRequest, libriflow::v1::WaitImportJobResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.WaitImportJob(typedRequest);
            });
    case EPipeMethod::CancelImportJob:
        return DispatchTyped<libriflow::v1::CancelImportJobRequest, libriflow::v1::CancelImportJobResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.CancelImportJob(typedRequest);
            });
    case EPipeMethod::RemoveImportJob:
        return DispatchTyped<libriflow::v1::RemoveImportJobRequest, libriflow::v1::RemoveImportJobResponse>(
            request,
            [this](const auto& typedRequest) {
                return m_adapter.RemoveImportJob(typedRequest);
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

} // namespace LibriFlow::PipeTransport
