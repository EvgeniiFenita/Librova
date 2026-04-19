#pragma once

#include <filesystem>

namespace Librova::Foundation {

// Creates directory hierarchy; throws std::runtime_error on failure.
void EnsureDirectory(const std::filesystem::path& path);

// Removes path and all children recursively without throwing.
// Silently ignores empty paths and filesystem errors.
void RemovePathNoThrow(const std::filesystem::path& path) noexcept;

} // namespace Librova::Foundation
