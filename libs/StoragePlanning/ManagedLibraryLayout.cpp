#include "StoragePlanning/ManagedLibraryLayout.hpp"

#include <format>
#include <stdexcept>

#include "Domain/BookFormat.hpp"

namespace Librova::StoragePlanning {

SLibraryLayoutPaths CManagedLibraryLayout::Build(const std::filesystem::path& libraryRoot)
{
    return {
        .Root = libraryRoot,
        .DatabaseDirectory = libraryRoot / "Database",
        .BooksDirectory = libraryRoot / "Books",
        .CoversDirectory = libraryRoot / "Covers",
        .TempDirectory = libraryRoot / "Temp",
        .TrashDirectory = libraryRoot / "Trash",
        .LogsDirectory = libraryRoot / "Logs"
    };
}

std::string CManagedLibraryLayout::GetBookFolderName(const Librova::Domain::SBookId bookId)
{
    if (!bookId.IsValid())
    {
        throw std::invalid_argument("Book id must be positive.");
    }

    return std::format("{:010}", bookId.Value);
}

std::filesystem::path CManagedLibraryLayout::GetBookDirectory(
    const std::filesystem::path& libraryRoot,
    const Librova::Domain::SBookId bookId)
{
    return Build(libraryRoot).BooksDirectory / GetBookFolderName(bookId);
}

std::filesystem::path CManagedLibraryLayout::GetManagedBookPath(
    const std::filesystem::path& libraryRoot,
    const Librova::Domain::SBookId bookId,
    const Librova::Domain::EBookFormat format)
{
    return GetBookDirectory(libraryRoot, bookId) / Librova::Domain::GetManagedFileName(format);
}

std::filesystem::path CManagedLibraryLayout::GetCoverPath(
    const std::filesystem::path& libraryRoot,
    const Librova::Domain::SBookId bookId,
    std::string_view extension)
{
    if (extension.empty())
    {
        throw std::invalid_argument("Cover extension must not be empty.");
    }

    const std::string normalizedExtension = extension.front() == '.'
        ? std::string{extension.substr(1)}
        : std::string{extension};

    return Build(libraryRoot).CoversDirectory / std::format("{}.{}", GetBookFolderName(bookId), normalizedExtension);
}

std::filesystem::path CManagedLibraryLayout::GetStagingDirectory(
    const std::filesystem::path& libraryRoot,
    const Librova::Domain::SBookId bookId)
{
    return Build(libraryRoot).TempDirectory / GetBookFolderName(bookId);
}

} // namespace Librova::StoragePlanning
