#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <google/protobuf/message_lite.h>

#include "PipeTransport/NamedPipeChannel.hpp"
#include "PipeTransport/PipeProtocol.hpp"

namespace LibriFlow::PipeClient {

class CPipeTransportError : public std::runtime_error
{
public:
    explicit CPipeTransportError(const std::string& message);
};

class CNamedPipeClient final
{
public:
    explicit CNamedPipeClient(std::filesystem::path pipePath);

    template <typename TRequest, typename TResponse>
    [[nodiscard]] TResponse Call(
        const LibriFlow::PipeTransport::EPipeMethod method,
        const TRequest& request,
        const std::chrono::milliseconds timeout) const
    {
        static_assert(std::is_base_of_v<google::protobuf::MessageLite, TRequest>);
        static_assert(std::is_base_of_v<google::protobuf::MessageLite, TResponse>);

        std::string requestPayload;
        if (!request.SerializeToString(&requestPayload))
        {
            throw CPipeTransportError("Failed to serialize protobuf request.");
        }

        const auto startTime = std::chrono::steady_clock::now();
        auto connection = LibriFlow::PipeTransport::ConnectToNamedPipe(m_pipePath, timeout);
        const LibriFlow::PipeTransport::SPipeRequestEnvelope envelope{
            .RequestId = NextRequestId(),
            .Method = method,
            .Payload = std::move(requestPayload)
        };

        connection.WriteMessage(LibriFlow::PipeTransport::SerializeRequestEnvelope(envelope));

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime);
        if (elapsed >= timeout)
        {
            throw CPipeTransportError("Pipe call timed out.");
        }

        const auto responseBytes = connection.ReadMessage(timeout - elapsed);
        const auto parsedResponse = LibriFlow::PipeTransport::DeserializeResponseEnvelope(responseBytes);
        if (!parsedResponse.HasValue())
        {
            throw CPipeTransportError(parsedResponse.Error);
        }

        if (parsedResponse.Value->RequestId != envelope.RequestId)
        {
            throw CPipeTransportError("Received mismatched pipe response id.");
        }

        if (parsedResponse.Value->Status != LibriFlow::PipeTransport::EPipeResponseStatus::Ok)
        {
            throw CPipeTransportError(
                parsedResponse.Value->ErrorMessage.empty()
                    ? "Pipe request failed."
                    : parsedResponse.Value->ErrorMessage);
        }

        TResponse response;
        if (!response.ParseFromString(parsedResponse.Value->Payload))
        {
            throw CPipeTransportError("Failed to parse protobuf response.");
        }

        return response;
    }

private:
    [[nodiscard]] static std::uint64_t NextRequestId() noexcept;

    std::filesystem::path m_pipePath;
};

} // namespace LibriFlow::PipeClient
