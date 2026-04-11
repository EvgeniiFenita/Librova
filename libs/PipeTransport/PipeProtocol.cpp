#include "PipeTransport/PipeProtocol.hpp"

#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace Librova::PipeTransport {
namespace {

constexpr std::uint32_t RequestMagic = 0x4C465250; // LFRP
constexpr std::uint32_t ResponseMagic = 0x4C465253; // LFRS
constexpr std::uint32_t ProtocolVersion = 1;

void AppendU32(std::vector<std::byte>& bytes, const std::uint32_t value)
{
    const auto raw = std::bit_cast<std::array<std::byte, sizeof(value)>>(value);
    bytes.insert(bytes.end(), raw.begin(), raw.end());
}

void AppendU64(std::vector<std::byte>& bytes, const std::uint64_t value)
{
    const auto raw = std::bit_cast<std::array<std::byte, sizeof(value)>>(value);
    bytes.insert(bytes.end(), raw.begin(), raw.end());
}

[[nodiscard]] TPipeParseResult<std::uint32_t> ReadU32(
    const std::vector<std::byte>& bytes,
    std::size_t& offset)
{
    if (offset + sizeof(std::uint32_t) > bytes.size())
    {
        return {.Error = "Unexpected end of message while reading uint32."};
    }

    std::array<std::byte, sizeof(std::uint32_t)> raw{};
    std::memcpy(raw.data(), bytes.data() + offset, sizeof(std::uint32_t));
    offset += sizeof(std::uint32_t);
    return {.Value = std::bit_cast<std::uint32_t>(raw)};
}

[[nodiscard]] TPipeParseResult<std::uint64_t> ReadU64(
    const std::vector<std::byte>& bytes,
    std::size_t& offset)
{
    if (offset + sizeof(std::uint64_t) > bytes.size())
    {
        return {.Error = "Unexpected end of message while reading uint64."};
    }

    std::array<std::byte, sizeof(std::uint64_t)> raw{};
    std::memcpy(raw.data(), bytes.data() + offset, sizeof(std::uint64_t));
    offset += sizeof(std::uint64_t);
    return {.Value = std::bit_cast<std::uint64_t>(raw)};
}

[[nodiscard]] TPipeParseResult<std::string> ReadString(
    const std::vector<std::byte>& bytes,
    std::size_t& offset)
{
    const auto lengthResult = ReadU32(bytes, offset);
    if (!lengthResult.HasValue())
    {
        return {.Error = lengthResult.Error};
    }

    const auto length = static_cast<std::size_t>(*lengthResult.Value);
    if (offset + length > bytes.size())
    {
        return {.Error = "Unexpected end of message while reading string."};
    }

    std::string value(length, '\0');
    if (length != 0)
    {
        std::memcpy(value.data(), bytes.data() + offset, length);
    }

    offset += length;
    return {.Value = std::move(value)};
}

void AppendString(std::vector<std::byte>& bytes, const std::string& value)
{
    if (value.size() > std::numeric_limits<std::uint32_t>::max())
    {
        throw std::runtime_error("Pipe payload exceeds uint32 framing limit.");
    }

    AppendU32(bytes, static_cast<std::uint32_t>(value.size()));
    if (!value.empty())
    {
        const auto* first = reinterpret_cast<const std::byte*>(value.data());
        bytes.insert(bytes.end(), first, first + value.size());
    }
}

template <typename TEnum>
[[nodiscard]] bool IsKnownEnumValue(const std::uint32_t value)
{
    switch (static_cast<TEnum>(value))
    {
    case TEnum::StartImport:
    case TEnum::ListBooks:
    case TEnum::GetBookDetails:
    case TEnum::ExportBook:
    case TEnum::MoveBookToTrash:
    case TEnum::GetImportJobSnapshot:
    case TEnum::GetImportJobResult:
    case TEnum::WaitImportJob:
    case TEnum::CancelImportJob:
    case TEnum::RemoveImportJob:
    case TEnum::ValidateImportSources:
        return true;
    default:
        return false;
    }
}

template <>
[[nodiscard]] bool IsKnownEnumValue<EPipeResponseStatus>(const std::uint32_t value)
{
    switch (static_cast<EPipeResponseStatus>(value))
    {
    case EPipeResponseStatus::Ok:
    case EPipeResponseStatus::InvalidRequest:
    case EPipeResponseStatus::UnknownMethod:
    case EPipeResponseStatus::InternalError:
        return true;
    default:
        return false;
    }
}

} // namespace

std::vector<std::byte> SerializeRequestEnvelope(const SPipeRequestEnvelope& envelope)
{
    std::vector<std::byte> bytes;
    bytes.reserve(32 + envelope.Payload.size());
    AppendU32(bytes, RequestMagic);
    AppendU32(bytes, ProtocolVersion);
    AppendU64(bytes, envelope.RequestId);
    AppendU32(bytes, static_cast<std::uint32_t>(envelope.Method));
    AppendString(bytes, envelope.Payload);
    return bytes;
}

std::vector<std::byte> SerializeResponseEnvelope(const SPipeResponseEnvelope& envelope)
{
    std::vector<std::byte> bytes;
    bytes.reserve(40 + envelope.Payload.size() + envelope.ErrorMessage.size());
    AppendU32(bytes, ResponseMagic);
    AppendU32(bytes, ProtocolVersion);
    AppendU64(bytes, envelope.RequestId);
    AppendU32(bytes, static_cast<std::uint32_t>(envelope.Status));
    AppendString(bytes, envelope.Payload);
    AppendString(bytes, envelope.ErrorMessage);
    return bytes;
}

TPipeParseResult<SPipeRequestEnvelope> DeserializeRequestEnvelope(const std::vector<std::byte>& bytes)
{
    std::size_t offset = 0;

    const auto magicResult = ReadU32(bytes, offset);
    if (!magicResult.HasValue())
    {
        return {.Error = magicResult.Error};
    }

    if (*magicResult.Value != RequestMagic)
    {
        return {.Error = "Unsupported request envelope magic."};
    }

    const auto versionResult = ReadU32(bytes, offset);
    if (!versionResult.HasValue())
    {
        return {.Error = versionResult.Error};
    }

    if (*versionResult.Value != ProtocolVersion)
    {
        return {.Error = "Unsupported request envelope version."};
    }

    const auto requestIdResult = ReadU64(bytes, offset);
    if (!requestIdResult.HasValue())
    {
        return {.Error = requestIdResult.Error};
    }

    const auto methodResult = ReadU32(bytes, offset);
    if (!methodResult.HasValue())
    {
        return {.Error = methodResult.Error};
    }

    if (!IsKnownEnumValue<EPipeMethod>(*methodResult.Value))
    {
        return {.Error = "Unknown pipe method."};
    }

    const auto payloadResult = ReadString(bytes, offset);
    if (!payloadResult.HasValue())
    {
        return {.Error = payloadResult.Error};
    }

    if (offset != bytes.size())
    {
        return {.Error = "Request envelope contains trailing bytes."};
    }

    return {
        .Value = SPipeRequestEnvelope{
            .RequestId = *requestIdResult.Value,
            .Method = static_cast<EPipeMethod>(*methodResult.Value),
            .Payload = *payloadResult.Value
        }
    };
}

TPipeParseResult<SPipeResponseEnvelope> DeserializeResponseEnvelope(const std::vector<std::byte>& bytes)
{
    std::size_t offset = 0;

    const auto magicResult = ReadU32(bytes, offset);
    if (!magicResult.HasValue())
    {
        return {.Error = magicResult.Error};
    }

    if (*magicResult.Value != ResponseMagic)
    {
        return {.Error = "Unsupported response envelope magic."};
    }

    const auto versionResult = ReadU32(bytes, offset);
    if (!versionResult.HasValue())
    {
        return {.Error = versionResult.Error};
    }

    if (*versionResult.Value != ProtocolVersion)
    {
        return {.Error = "Unsupported response envelope version."};
    }

    const auto requestIdResult = ReadU64(bytes, offset);
    if (!requestIdResult.HasValue())
    {
        return {.Error = requestIdResult.Error};
    }

    const auto statusResult = ReadU32(bytes, offset);
    if (!statusResult.HasValue())
    {
        return {.Error = statusResult.Error};
    }

    if (!IsKnownEnumValue<EPipeResponseStatus>(*statusResult.Value))
    {
        return {.Error = "Unknown response status."};
    }

    const auto payloadResult = ReadString(bytes, offset);
    if (!payloadResult.HasValue())
    {
        return {.Error = payloadResult.Error};
    }

    const auto errorResult = ReadString(bytes, offset);
    if (!errorResult.HasValue())
    {
        return {.Error = errorResult.Error};
    }

    if (offset != bytes.size())
    {
        return {.Error = "Response envelope contains trailing bytes."};
    }

    return {
        .Value = SPipeResponseEnvelope{
            .RequestId = *requestIdResult.Value,
            .Status = static_cast<EPipeResponseStatus>(*statusResult.Value),
            .Payload = *payloadResult.Value,
            .ErrorMessage = *errorResult.Value
        }
    };
}

} // namespace Librova::PipeTransport
