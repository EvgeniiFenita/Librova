#pragma once

#include <filesystem>
#include <string>

#include "Domain/BookFormat.hpp"
#include "Domain/BookId.hpp"
#include "Domain/StorageEncoding.hpp"

namespace Librova::StoragePlanning {

struct SLibraryLayoutPaths
{
    std::filesystem::path Root;
    std::filesystem::path DatabaseDirectory;
    std::filesystem::path ObjectsDirectory;
    std::filesystem::path TrashDirectory;
    std::filesystem::path LogsDirectory;
};

class CManagedLibraryLayout
{
public:
    [[nodiscard]] static SLibraryLayoutPaths Build(const std::filesystem::path& libraryRoot);
    [[nodiscard]] static std::string GetBookFolderName(Librova::Domain::SBookId bookId);
    [[nodiscard]] static std::filesystem::path GetObjectShardDirectory(
        const std::filesystem::path& libraryRoot,
        Librova::Domain::SBookId bookId);
    [[nodiscard]] static std::filesystem::path GetManagedBookPath(
        const std::filesystem::path& libraryRoot,
        Librova::Domain::SBookId bookId,
        Librova::Domain::EBookFormat format);
    [[nodiscard]] static std::filesystem::path GetManagedBookPath(
        const std::filesystem::path& libraryRoot,
        Librova::Domain::SBookId bookId,
        Librova::Domain::EBookFormat format,
        Librova::Domain::EStorageEncoding storageEncoding);
    [[nodiscard]] static std::filesystem::path GetCoverPath(
        const std::filesystem::path& libraryRoot,
        Librova::Domain::SBookId bookId,
        std::string_view extension);
};

} // namespace Librova::StoragePlanning
