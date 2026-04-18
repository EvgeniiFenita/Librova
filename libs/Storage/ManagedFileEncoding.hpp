#pragma once

#include <filesystem>

#include "Domain/StorageEncoding.hpp"

namespace Librova::ManagedFileEncoding {

void EncodeFileToPath(
    const std::filesystem::path& sourcePath,
    Librova::Domain::EStorageEncoding encoding,
    const std::filesystem::path& destinationPath);

void DecodeFileToPath(
    const std::filesystem::path& sourcePath,
    Librova::Domain::EStorageEncoding encoding,
    const std::filesystem::path& destinationPath);

} // namespace Librova::ManagedFileEncoding
