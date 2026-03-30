#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>

#include "PipeTransport/NamedPipeChannel.hpp"
#include "PipeTransport/PipeProtocol.hpp"

namespace {

std::filesystem::path BuildTestPipePath()
{
    const auto uniqueId = std::to_wstring(
        static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
    return std::filesystem::path{std::wstring{LR"(\\.\pipe\LibriFlow.Test.)"} + uniqueId};
}

} // namespace

TEST_CASE("Named pipe channel exchanges framed request and response payloads", "[pipe]")
{
    const auto pipePath = BuildTestPipePath();
    std::exception_ptr serverFailure;

    std::jthread serverThread([&pipePath, &serverFailure] {
        try
        {
            LibriFlow::PipeTransport::CNamedPipeServer server(pipePath);
            auto connection = server.WaitForClient();

            const auto requestBytes = connection.ReadMessage();
            const auto parsedRequest = LibriFlow::PipeTransport::DeserializeRequestEnvelope(requestBytes);
            if (!parsedRequest.HasValue())
            {
                throw std::runtime_error(parsedRequest.Error);
            }

            if (parsedRequest.Value->RequestId != 11 || parsedRequest.Value->Payload != "ping")
            {
                throw std::runtime_error("Server received unexpected named pipe request payload.");
            }

            const LibriFlow::PipeTransport::SPipeResponseEnvelope response{
                .RequestId = parsedRequest.Value->RequestId,
                .Status = LibriFlow::PipeTransport::EPipeResponseStatus::Ok,
                .Payload = "pong"
            };

            connection.WriteMessage(LibriFlow::PipeTransport::SerializeResponseEnvelope(response));
        }
        catch (...)
        {
            serverFailure = std::current_exception();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    auto client = LibriFlow::PipeTransport::ConnectToNamedPipe(pipePath, std::chrono::seconds(2));

    const LibriFlow::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 11,
        .Method = LibriFlow::PipeTransport::EPipeMethod::WaitImportJob,
        .Payload = "ping"
    };

    client.WriteMessage(LibriFlow::PipeTransport::SerializeRequestEnvelope(request));

    const auto responseBytes = client.ReadMessage();
    const auto parsedResponse = LibriFlow::PipeTransport::DeserializeResponseEnvelope(responseBytes);
    REQUIRE(parsedResponse.HasValue());
    REQUIRE(parsedResponse.Value->RequestId == request.RequestId);
    REQUIRE(parsedResponse.Value->Status == LibriFlow::PipeTransport::EPipeResponseStatus::Ok);
    REQUIRE(parsedResponse.Value->Payload == "pong");

    serverThread.join();
    REQUIRE(serverFailure == nullptr);
}
