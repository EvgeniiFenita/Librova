#pragma once

#include <filesystem>
#include <string>

namespace Librova::Hashing {

/// Computes the SHA-256 hex digest of the given file.
/// Throws std::runtime_error on I/O or Windows BCrypt API failures.
[[nodiscard]] std::string ComputeFileSha256Hex(const std::filesystem::path& filePath);

} // namespace Librova::Hashing
