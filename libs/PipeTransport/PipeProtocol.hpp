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
    MoveBookToTrash = 5,
    GetImportJobSnapshot = 6,
    GetImportJobResult = 7,
    WaitImportJob = 8,
    CancelImportJob = 9,
    RemoveImportJob = 10,
    GetLibraryStatistics = 11
};

enum class EPipeResponseStatus : std::uint32_t
{
    Ok = 0,
    InvalidRequest = 1,
    UnknownMethod = 2,
    InternalError = 3
};

static_assert(static_cast<std::uint32_t>(EPipeMethod::StartImport) == 1);
static_assert(static_cast<std::uint32_t>(EPipeMethod::ListBooks) == 2);
static_assert(static_cast<std::uint32_t>(EPipeMethod::GetBookDetails) == 3);
static_assert(static_cast<std::uint32_t>(EPipeMethod::ExportBook) == 4);
static_assert(static_cast<std::uint32_t>(EPipeMethod::MoveBookToTrash) == 5);
static_assert(static_cast<std::uint32_t>(EPipeMethod::GetImportJobSnapshot) == 6);
static_assert(static_cast<std::uint32_t>(EPipeMethod::GetImportJobResult) == 7);
static_assert(static_cast<std::uint32_t>(EPipeMethod::WaitImportJob) == 8);
static_assert(static_cast<std::uint32_t>(EPipeMethod::CancelImportJob) == 9);
static_assert(static_cast<std::uint32_t>(EPipeMethod::RemoveImportJob) == 10);
static_assert(static_cast<std::uint32_t>(EPipeMethod::GetLibraryStatistics) == 11);

static_assert(static_cast<std::uint32_t>(EPipeResponseStatus::Ok) == 0);
static_assert(static_cast<std::uint32_t>(EPipeResponseStatus::InvalidRequest) == 1);
static_assert(static_cast<std::uint32_t>(EPipeResponseStatus::UnknownMethod) == 2);
static_assert(static_cast<std::uint32_t>(EPipeResponseStatus::InternalError) == 3);

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
