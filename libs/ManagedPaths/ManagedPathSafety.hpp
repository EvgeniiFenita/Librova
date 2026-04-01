#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace Librova::ManagedPaths {

[[nodiscard]] std::filesystem::path ResolveExistingPathWithinRoot(
    const std::filesystem::path& root,
    const std::filesystem::path& managedPath,
    std::string_view missingPathMessage,
    std::string_view unsafePathMessage,
    std::string_view canonicalizationErrorMessage);

[[nodiscard]] std::string PathToUtf8(const std::filesystem::path& path);

} // namespace Librova::ManagedPaths
