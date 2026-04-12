#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Domain/BookFormat.hpp"
#include "Domain/BookId.hpp"
#include "Domain/StorageEncoding.hpp"

namespace Librova::Domain {

struct SBookMetadata
{
    std::string TitleUtf8;
    std::vector<std::string> AuthorsUtf8;
    std::string Language;
    std::optional<std::string> SeriesUtf8;
    std::optional<double> SeriesIndex;
    std::optional<std::string> PublisherUtf8;
    std::optional<int> Year;
    std::optional<std::string> Isbn;
    std::vector<std::string> TagsUtf8;
    std::vector<std::string> GenresUtf8;
    std::optional<std::string> DescriptionUtf8;
    std::optional<std::string> Identifier;

    [[nodiscard]] bool HasTitle() const noexcept
    {
        return !TitleUtf8.empty();
    }

    [[nodiscard]] bool HasAuthors() const noexcept
    {
        return !AuthorsUtf8.empty();
    }
};

struct SBookFileInfo
{
    EBookFormat Format = EBookFormat::Epub;
    EStorageEncoding StorageEncoding = EStorageEncoding::Plain;
    std::filesystem::path ManagedPath;
    std::uintmax_t SizeBytes = 0;
    std::string Sha256Hex;

    [[nodiscard]] bool HasManagedPath() const noexcept
    {
        return !ManagedPath.empty();
    }

    [[nodiscard]] bool HasHash() const noexcept
    {
        return !Sha256Hex.empty();
    }

    [[nodiscard]] bool IsCompressed() const noexcept
    {
        return StorageEncoding == EStorageEncoding::Compressed;
    }
};

struct SBook
{
    SBookId Id;
    SBookMetadata Metadata;
    SBookFileInfo File;
    std::optional<std::filesystem::path> CoverPath;
    std::chrono::system_clock::time_point AddedAtUtc{};

    [[nodiscard]] bool HasIdentity() const noexcept
    {
        return Id.IsValid();
    }

    [[nodiscard]] bool HasCover() const noexcept
    {
        return CoverPath.has_value() && !CoverPath->empty();
    }
};

} // namespace Librova::Domain
