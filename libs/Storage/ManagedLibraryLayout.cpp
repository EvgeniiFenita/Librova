#include "Storage/ManagedLibraryLayout.hpp"

#include <cstdint>
#include <format>
#include <stdexcept>

#include "Domain/BookFormat.hpp"

namespace Librova::StoragePlanning {

namespace {

constexpr std::uint32_t Fnv1aOffsetBasis32 = 2166136261u;
constexpr std::uint32_t Fnv1aPrime32 = 16777619u;

std::uint32_t ComputeBookShardHash(std::string_view bookFolderName)
{
    std::uint32_t hash = Fnv1aOffsetBasis32;
    for (const unsigned char ch : bookFolderName)
    {
        hash ^= ch;
        hash *= Fnv1aPrime32;
    }

    return hash;
}

std::string FormatShardBucket(const std::uint32_t hash, const std::uint32_t shift)
{
    return std::format("{:02x}", static_cast<unsigned>((hash >> shift) & 0xffu));
}

} // namespace

SLibraryLayoutPaths CManagedLibraryLayout::Build(const std::filesystem::path& libraryRoot)
{
    return {
        .Root = libraryRoot,
        .DatabaseDirectory = libraryRoot / "Database",
        .ObjectsDirectory = libraryRoot / "Objects",
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

std::filesystem::path CManagedLibraryLayout::GetObjectShardDirectory(
    const std::filesystem::path& libraryRoot,
    const Librova::Domain::SBookId bookId)
{
    const std::string bookFolderName = GetBookFolderName(bookId);
    const std::uint32_t hash = ComputeBookShardHash(bookFolderName);
    return Build(libraryRoot).ObjectsDirectory
        / FormatShardBucket(hash, 0)
        / FormatShardBucket(hash, 8);
}

std::filesystem::path CManagedLibraryLayout::GetManagedBookPath(
    const std::filesystem::path& libraryRoot,
    const Librova::Domain::SBookId bookId,
    const Librova::Domain::EBookFormat format)
{
    const std::string bookFolderName = GetBookFolderName(bookId);
    return GetObjectShardDirectory(libraryRoot, bookId)
        / std::format("{}.{}", bookFolderName, Librova::Domain::GetManagedFileName(format));
}

std::filesystem::path CManagedLibraryLayout::GetManagedBookPath(
    const std::filesystem::path& libraryRoot,
    const Librova::Domain::SBookId bookId,
    const Librova::Domain::EBookFormat format,
    const Librova::Domain::EStorageEncoding storageEncoding)
{
    const std::string bookFolderName = GetBookFolderName(bookId);
    return GetObjectShardDirectory(libraryRoot, bookId)
        / std::format("{}.{}", bookFolderName, Librova::Domain::GetManagedFileName(format, storageEncoding));
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

    const std::string bookFolderName = GetBookFolderName(bookId);
    return GetObjectShardDirectory(libraryRoot, bookId)
        / std::format("{}.cover.{}", bookFolderName, normalizedExtension);
}

} // namespace Librova::StoragePlanning
