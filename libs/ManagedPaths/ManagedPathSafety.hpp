#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace Librova::ManagedPaths {

[[nodiscard]] bool IsSafeRelativeManagedPath(const std::filesystem::path& path);

[[nodiscard]] std::optional<std::filesystem::path> TryResolvePathWithinRoot(
    const std::filesystem::path& root,
    const std::filesystem::path& managedPath,
    std::string_view unsafePathMessage,
    std::string_view canonicalizationErrorMessage);

[[nodiscard]] std::filesystem::path ResolveExistingPathWithinRoot(
    const std::filesystem::path& root,
    const std::filesystem::path& managedPath,
    std::string_view missingPathMessage,
    std::string_view unsafePathMessage,
    std::string_view canonicalizationErrorMessage);

} // namespace Librova::ManagedPaths
