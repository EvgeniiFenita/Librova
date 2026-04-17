#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <array>
#include <bit>
#include <chrono>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>

#include "PipeTransport/NamedPipeChannel.hpp"
#include "PipeTransport/PipeProtocol.hpp"
#include "TestNamedPipeReadySignal.hpp"

namespace {

std::filesystem::path BuildTestPipePath()
{
    const auto uniqueId = std::to_wstring(
        static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
    return std::filesystem::path{std::wstring{LR"(\\.\pipe\Librova.Test.)"} + uniqueId};
}

} // namespace

TEST_CASE("Named pipe channel exchanges framed request and response payloads", "[pipe]")
{
    const auto pipePath = BuildTestPipePath();
    std::exception_ptr serverFailure;
    CTestNamedPipeReadySignal readySignal;

    std::jthread serverThread([&pipePath, &readySignal, &serverFailure] {
        try
        {
            Librova::PipeTransport::CNamedPipeServer server(pipePath);
            readySignal.NotifyReady();
            auto connection = server.WaitForClient();

            const auto requestBytes = connection.ReadMessage();
            const auto parsedRequest = Librova::PipeTransport::DeserializeRequestEnvelope(requestBytes);
            if (!parsedRequest.HasValue())
            {
                throw std::runtime_error(parsedRequest.Error);
            }

            if (parsedRequest.Value->RequestId != 11 || parsedRequest.Value->Payload != "ping")
            {
                throw std::runtime_error("Server received unexpected named pipe request payload.");
            }

            const Librova::PipeTransport::SPipeResponseEnvelope response{
                .RequestId = parsedRequest.Value->RequestId,
                .Status = Librova::PipeTransport::EPipeResponseStatus::Ok,
                .Payload = "pong"
            };

            connection.WriteMessage(Librova::PipeTransport::SerializeResponseEnvelope(response));
        }
        catch (...)
        {
            const std::exception_ptr failure = std::current_exception();
            readySignal.NotifyFailure(failure);
            serverFailure = failure;
        }
    });

    readySignal.Wait();

    auto client = Librova::PipeTransport::ConnectToNamedPipe(pipePath, std::chrono::seconds(2));

    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 11,
        .Method = Librova::PipeTransport::EPipeMethod::WaitImportJob,
        .Payload = "ping"
    };

    client.WriteMessage(Librova::PipeTransport::SerializeRequestEnvelope(request));

    const auto responseBytes = client.ReadMessage();
    const auto parsedResponse = Librova::PipeTransport::DeserializeResponseEnvelope(responseBytes);
    REQUIRE(parsedResponse.HasValue());
    REQUIRE(parsedResponse.Value->RequestId == request.RequestId);
    REQUIRE(parsedResponse.Value->Status == Librova::PipeTransport::EPipeResponseStatus::Ok);
    REQUIRE(parsedResponse.Value->Payload == "pong");

    serverThread.join();
    REQUIRE(serverFailure == nullptr);
}

TEST_CASE("Named pipe channel rejects oversized framed payload before allocation", "[pipe]")
{
    const auto pipePath = BuildTestPipePath();
    std::exception_ptr serverFailure;
    CTestNamedPipeReadySignal readySignal;

    std::jthread serverThread([&pipePath, &readySignal, &serverFailure] {
        try
        {
            Librova::PipeTransport::CNamedPipeServer server(pipePath);
            readySignal.NotifyReady();
            auto connection = server.WaitForClient();
            REQUIRE_THROWS_WITH(
                connection.ReadMessage(),
                Catch::Matchers::ContainsSubstring("maximum size"));
        }
        catch (...)
        {
            const std::exception_ptr failure = std::current_exception();
            readySignal.NotifyFailure(failure);
            serverFailure = failure;
        }
    });

    readySignal.Wait();

    const HANDLE clientHandle = CreateFileW(
        pipePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);
    REQUIRE(clientHandle != INVALID_HANDLE_VALUE);

    const auto oversizedLength = static_cast<std::uint32_t>(Librova::PipeTransport::MaxPipeFrameBytes + 1U);
    const auto rawLength = std::bit_cast<std::array<std::byte, sizeof(oversizedLength)>>(oversizedLength);
    DWORD bytesWritten = 0;
    REQUIRE(WriteFile(clientHandle, rawLength.data(), static_cast<DWORD>(rawLength.size()), &bytesWritten, nullptr) != FALSE);
    REQUIRE(bytesWritten == rawLength.size());
    CloseHandle(clientHandle);

    serverThread.join();
    REQUIRE(serverFailure == nullptr);
}

TEST_CASE("Named pipe channel timeout read cancels pending overlapped wait instead of waiting for a late response", "[pipe]")
{
    const auto pipePath = BuildTestPipePath();
    std::exception_ptr serverFailure;
    CTestNamedPipeReadySignal readySignal;

    std::jthread serverThread([&pipePath, &readySignal, &serverFailure] {
        try
        {
            Librova::PipeTransport::CNamedPipeServer server(pipePath);
            readySignal.NotifyReady();
            auto connection = server.WaitForClient();
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            connection.WriteMessage({std::byte{0x42}});
        }
        catch (...)
        {
            const std::exception_ptr failure = std::current_exception();
            readySignal.NotifyFailure(failure);
            serverFailure = failure;
        }
    });

    readySignal.Wait();

    auto client = Librova::PipeTransport::ConnectToNamedPipe(pipePath, std::chrono::seconds(2));
    const auto startTime = std::chrono::steady_clock::now();

    REQUIRE_THROWS_WITH(
        client.ReadMessage(std::chrono::milliseconds(50)),
        Catch::Matchers::ContainsSubstring("Timed out waiting for named pipe data."));

    const auto elapsed = std::chrono::steady_clock::now() - startTime;
    REQUIRE(elapsed < std::chrono::milliseconds(200));

    serverThread.join();
    REQUIRE(serverFailure == nullptr);
}

TEST_CASE("Named pipe channel timeout write cancels pending overlapped wait instead of blocking behind a non-reading peer", "[pipe]")
{
    const auto pipePath = BuildTestPipePath();
    std::exception_ptr serverFailure;
    CTestNamedPipeReadySignal readySignal;

    std::jthread serverThread([&pipePath, &readySignal, &serverFailure] {
        try
        {
            Librova::PipeTransport::CNamedPipeServer server(pipePath);
            readySignal.NotifyReady();
            auto connection = server.WaitForClient();
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        catch (...)
        {
            const std::exception_ptr failure = std::current_exception();
            readySignal.NotifyFailure(failure);
            serverFailure = failure;
        }
    });

    readySignal.Wait();

    auto client = Librova::PipeTransport::ConnectToNamedPipe(pipePath, std::chrono::seconds(2));
    std::vector<std::byte> payload(2U * 1024U * 1024U, std::byte{0x5A});
    const auto startTime = std::chrono::steady_clock::now();

    REQUIRE_THROWS_WITH(
        client.WriteMessage(payload, std::chrono::milliseconds(50)),
        Catch::Matchers::ContainsSubstring("Timed out writing named pipe data."));

    const auto elapsed = std::chrono::steady_clock::now() - startTime;
    REQUIRE(elapsed < std::chrono::milliseconds(200));

    serverThread.join();
    REQUIRE(serverFailure == nullptr);
}

