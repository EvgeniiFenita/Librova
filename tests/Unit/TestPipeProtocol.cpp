#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

#include "PipeTransport/PipeProtocol.hpp"

TEST_CASE("Pipe request envelopes round-trip through binary framing", "[pipe]")
{
    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 42,
        .Method = Librova::PipeTransport::EPipeMethod::WaitImportJob,
        .Payload = "payload"
    };

    const auto bytes = Librova::PipeTransport::SerializeRequestEnvelope(request);
    const auto parsed = Librova::PipeTransport::DeserializeRequestEnvelope(bytes);

    REQUIRE(parsed.HasValue());
    REQUIRE(parsed.Value->RequestId == request.RequestId);
    REQUIRE(parsed.Value->Method == request.Method);
    REQUIRE(parsed.Value->Payload == request.Payload);
}

TEST_CASE("Pipe response envelopes round-trip through binary framing", "[pipe]")
{
    const Librova::PipeTransport::SPipeResponseEnvelope response{
        .RequestId = 77,
        .Status = Librova::PipeTransport::EPipeResponseStatus::InvalidRequest,
        .Payload = "response",
        .ErrorMessage = "broken payload"
    };

    const auto bytes = Librova::PipeTransport::SerializeResponseEnvelope(response);
    const auto parsed = Librova::PipeTransport::DeserializeResponseEnvelope(bytes);

    REQUIRE(parsed.HasValue());
    REQUIRE(parsed.Value->RequestId == response.RequestId);
    REQUIRE(parsed.Value->Status == response.Status);
    REQUIRE(parsed.Value->Payload == response.Payload);
    REQUIRE(parsed.Value->ErrorMessage == response.ErrorMessage);
}

TEST_CASE("Pipe protocol rejects corrupted method values", "[pipe]")
{
    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 7,
        .Method = Librova::PipeTransport::EPipeMethod::StartImport,
        .Payload = {}
    };

    auto bytes = Librova::PipeTransport::SerializeRequestEnvelope(request);
    REQUIRE(bytes.size() >= 20);

    bytes[16] = std::byte{0xFF};
    bytes[17] = std::byte{0xFF};
    bytes[18] = std::byte{0xFF};
    bytes[19] = std::byte{0x7F};

    const auto parsed = Librova::PipeTransport::DeserializeRequestEnvelope(bytes);
    REQUIRE_FALSE(parsed.HasValue());
}
