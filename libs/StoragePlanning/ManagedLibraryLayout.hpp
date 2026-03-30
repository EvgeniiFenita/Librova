#pragma once

#include <filesystem>
#include <string>

#include "Domain/BookFormat.hpp"
#include "Domain/BookId.hpp"

namespace LibriFlow::StoragePlanning {

struct SLibraryLayoutPaths
{
    std::filesystem::path Root;
    std::filesystem::path DatabaseDirectory;
    std::filesystem::path BooksDirectory;
    std::filesystem::path CoversDirectory;
    std::filesystem::path TempDirectory;
    std::filesystem::path LogsDirectory;
};

class CManagedLibraryLayout
{
public:
    [[nodiscard]] static SLibraryLayoutPaths Build(const std::filesystem::path& libraryRoot);
    [[nodiscard]] static std::string GetBookFolderName(LibriFlow::Domain::SBookId bookId);
    [[nodiscard]] static std::filesystem::path GetBookDirectory(
        const std::filesystem::path& libraryRoot,
        LibriFlow::Domain::SBookId bookId);
    [[nodiscard]] static std::filesystem::path GetManagedBookPath(
        const std::filesystem::path& libraryRoot,
        LibriFlow::Domain::SBookId bookId,
        LibriFlow::Domain::EBookFormat format);
    [[nodiscard]] static std::filesystem::path GetCoverPath(
        const std::filesystem::path& libraryRoot,
        LibriFlow::Domain::SBookId bookId,
        std::string_view extension);
    [[nodiscard]] static std::filesystem::path GetStagingDirectory(
        const std::filesystem::path& libraryRoot,
        LibriFlow::Domain::SBookId bookId);
};

} // namespace LibriFlow::StoragePlanning
