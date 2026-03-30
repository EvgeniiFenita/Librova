#pragma once

#include <optional>
#include <string_view>

namespace LibriFlow::Domain {

enum class EBookFormat
{
    Epub,
    Fb2
};

[[nodiscard]] std::string_view ToString(EBookFormat format) noexcept;
[[nodiscard]] std::string_view GetManagedFileName(EBookFormat format) noexcept;
[[nodiscard]] std::optional<EBookFormat> TryParseBookFormat(std::string_view value) noexcept;

} // namespace LibriFlow::Domain
