#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace Librova::ManagedPaths {

struct SDirectoryCleanupResidue
{
    std::filesystem::path Path;
    std::string Reason;
};

[[nodiscard]] bool IsSafeRelativeManagedPath(const std::filesystem::path& path, bool requireNoRootComponents = false);

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

void RemovePathRecursivelyWithinRoot(
    const std::filesystem::path& root,
    const std::filesystem::path& managedPath,
    std::string_view missingPathMessage,
    std::string_view unsafePathMessage,
    std::string_view canonicalizationErrorMessage);

[[nodiscard]] std::optional<SDirectoryCleanupResidue> CleanupEmptyDirectoriesUpTo(
    const std::filesystem::path& startDirectory,
    const std::filesystem::path& stopDirectoryExclusive) noexcept;

} // namespace Librova::ManagedPaths
