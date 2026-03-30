#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "Domain/Book.hpp"

namespace LibriFlow::Domain {

[[nodiscard]] std::string NormalizeText(std::string_view value);
[[nodiscard]] std::optional<std::string> NormalizeOptionalText(const std::optional<std::string>& value);
[[nodiscard]] std::optional<std::string> NormalizeIsbn(const std::optional<std::string>& value);
[[nodiscard]] std::string BuildDuplicateKey(const SBookMetadata& metadata);

} // namespace LibriFlow::Domain
