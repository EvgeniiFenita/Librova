#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Transport/PipeProtocol.hpp"

TEST_CASE("Pipe method ids stay synchronized for the current checkpoint", "[pipe]")
{
    REQUIRE(static_cast<std::uint32_t>(Librova::PipeTransport::EPipeMethod::StartImport) == 1);
    REQUIRE(static_cast<std::uint32_t>(Librova::PipeTransport::EPipeMethod::ListBooks) == 2);
    REQUIRE(static_cast<std::uint32_t>(Librova::PipeTransport::EPipeMethod::GetBookDetails) == 3);
    REQUIRE(static_cast<std::uint32_t>(Librova::PipeTransport::EPipeMethod::ExportBook) == 4);
    REQUIRE(static_cast<std::uint32_t>(Librova::PipeTransport::EPipeMethod::MoveBookToTrash) == 5);
    REQUIRE(static_cast<std::uint32_t>(Librova::PipeTransport::EPipeMethod::GetImportJobSnapshot) == 6);
    REQUIRE(static_cast<std::uint32_t>(Librova::PipeTransport::EPipeMethod::GetImportJobResult) == 7);
    REQUIRE(static_cast<std::uint32_t>(Librova::PipeTransport::EPipeMethod::WaitImportJob) == 8);
    REQUIRE(static_cast<std::uint32_t>(Librova::PipeTransport::EPipeMethod::CancelImportJob) == 9);
    REQUIRE(static_cast<std::uint32_t>(Librova::PipeTransport::EPipeMethod::RemoveImportJob) == 10);
    REQUIRE(static_cast<std::uint32_t>(Librova::PipeTransport::EPipeMethod::ValidateImportSources) == 12);
}

TEST_CASE("Pipe response status ids stay fixed across checkpoints", "[pipe]")
{
    REQUIRE(static_cast<std::uint32_t>(Librova::PipeTransport::EPipeResponseStatus::Ok) == 0);
    REQUIRE(static_cast<std::uint32_t>(Librova::PipeTransport::EPipeResponseStatus::InvalidRequest) == 1);
    REQUIRE(static_cast<std::uint32_t>(Librova::PipeTransport::EPipeResponseStatus::UnknownMethod) == 2);
    REQUIRE(static_cast<std::uint32_t>(Librova::PipeTransport::EPipeResponseStatus::InternalError) == 3);
}

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

TEST_CASE("Pipe protocol round-trips empty payloads without treating them as EOF", "[pipe]")
{
    const Librova::PipeTransport::SPipeResponseEnvelope response{
        .RequestId = 91,
        .Status = Librova::PipeTransport::EPipeResponseStatus::Ok,
        .Payload = {},
        .ErrorMessage = {}
    };

    const auto bytes = Librova::PipeTransport::SerializeResponseEnvelope(response);
    const auto parsed = Librova::PipeTransport::DeserializeResponseEnvelope(bytes);

    REQUIRE(parsed.HasValue());
    REQUIRE(parsed.Value->RequestId == response.RequestId);
    REQUIRE(parsed.Value->Status == response.Status);
    REQUIRE(parsed.Value->Payload.empty());
    REQUIRE(parsed.Value->ErrorMessage.empty());
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

TEST_CASE("Pipe protocol recognizes list books method in request framing", "[pipe]")
{
    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 88,
        .Method = Librova::PipeTransport::EPipeMethod::ListBooks,
        .Payload = "catalog"
    };

    const auto bytes = Librova::PipeTransport::SerializeRequestEnvelope(request);
    const auto parsed = Librova::PipeTransport::DeserializeRequestEnvelope(bytes);

    REQUIRE(parsed.HasValue());
    REQUIRE(parsed.Value->RequestId == request.RequestId);
    REQUIRE(parsed.Value->Method == Librova::PipeTransport::EPipeMethod::ListBooks);
    REQUIRE(parsed.Value->Payload == "catalog");
}

TEST_CASE("Pipe protocol recognizes move-book-to-trash method in request framing", "[pipe]")
{
    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 89,
        .Method = Librova::PipeTransport::EPipeMethod::MoveBookToTrash,
        .Payload = "trash"
    };

    const auto bytes = Librova::PipeTransport::SerializeRequestEnvelope(request);
    const auto parsed = Librova::PipeTransport::DeserializeRequestEnvelope(bytes);

    REQUIRE(parsed.HasValue());
    REQUIRE(parsed.Value->RequestId == request.RequestId);
    REQUIRE(parsed.Value->Method == Librova::PipeTransport::EPipeMethod::MoveBookToTrash);
    REQUIRE(parsed.Value->Payload == "trash");
}

