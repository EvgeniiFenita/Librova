#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Librova::PipeTransport {

enum class EPipeMethod : std::uint32_t
{
    StartImport = 1,
    ListBooks = 2,
    GetBookDetails = 3,
    ExportBook = 4,
    GetImportJobSnapshot = 5,
    GetImportJobResult = 6,
    WaitImportJob = 7,
    CancelImportJob = 8,
    RemoveImportJob = 9
};

enum class EPipeResponseStatus : std::uint32_t
{
    Ok = 0,
    InvalidRequest = 1,
    UnknownMethod = 2,
    InternalError = 3
};

struct SPipeRequestEnvelope
{
    std::uint64_t RequestId = 0;
    EPipeMethod Method = EPipeMethod::StartImport;
    std::string Payload;
};

struct SPipeResponseEnvelope
{
    std::uint64_t RequestId = 0;
    EPipeResponseStatus Status = EPipeResponseStatus::Ok;
    std::string Payload;
    std::string ErrorMessage;
};

template <typename TValue>
struct TPipeParseResult
{
    std::optional<TValue> Value;
    std::string Error;

    [[nodiscard]] bool HasValue() const
    {
        return Value.has_value();
    }
};

[[nodiscard]] std::vector<std::byte> SerializeRequestEnvelope(const SPipeRequestEnvelope& envelope);
[[nodiscard]] std::vector<std::byte> SerializeResponseEnvelope(const SPipeResponseEnvelope& envelope);

[[nodiscard]] TPipeParseResult<SPipeRequestEnvelope> DeserializeRequestEnvelope(
    const std::vector<std::byte>& bytes);

[[nodiscard]] TPipeParseResult<SPipeResponseEnvelope> DeserializeResponseEnvelope(
    const std::vector<std::byte>& bytes);

} // namespace Librova::PipeTransport
