#pragma once

#include <optional>
#include <string_view>

namespace Librova::Domain {

enum class EStorageEncoding
{
    Plain,
    Compressed
};

[[nodiscard]] std::string_view ToString(EStorageEncoding encoding) noexcept;
[[nodiscard]] std::optional<EStorageEncoding> TryParseStorageEncoding(std::string_view value) noexcept;

} // namespace Librova::Domain
