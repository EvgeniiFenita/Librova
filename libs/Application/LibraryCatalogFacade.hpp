#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "Domain/BookRepository.hpp"

namespace Librova::Application {

struct SBookListRequest
{
    std::string TextUtf8;
    std::optional<std::string> AuthorUtf8;
    std::optional<std::string> Language;
    std::optional<std::string> SeriesUtf8;
    std::vector<std::string> TagsUtf8;
    std::optional<Librova::Domain::EBookFormat> Format;
    std::optional<Librova::Domain::EBookSort> SortBy;
    std::optional<Librova::Domain::ESortDirection> SortDirection;
    std::size_t Offset = 0;
    std::size_t Limit = 50;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return Limit > 0;
    }
};

struct SBookListItem
{
    Librova::Domain::SBookId Id;
    std::string TitleUtf8;
    std::vector<std::string> AuthorsUtf8;
    std::string Language;
    std::optional<std::string> SeriesUtf8;
    std::optional<double> SeriesIndex;
    std::optional<int> Year;
    std::vector<std::string> TagsUtf8;
    Librova::Domain::EBookFormat Format = Librova::Domain::EBookFormat::Epub;
    std::filesystem::path ManagedPath;
    std::optional<std::filesystem::path> CoverPath;
    std::uintmax_t SizeBytes = 0;
    std::chrono::system_clock::time_point AddedAtUtc{};
};

struct SLibraryStatistics
{
    std::uint64_t BookCount = 0;
    std::uint64_t TotalManagedBookSizeBytes = 0;
    std::uint64_t TotalLibrarySizeBytes = 0;
};

struct SBookListResult
{
    std::vector<SBookListItem> Items;
    std::uint64_t TotalCount = 0;
    std::vector<std::string> AvailableLanguages;
    SLibraryStatistics Statistics;

    [[nodiscard]] bool IsEmpty() const noexcept
    {
        return Items.empty();
    }
};

struct SBookDetails
{
    Librova::Domain::SBookId Id;
    std::string TitleUtf8;
    std::vector<std::string> AuthorsUtf8;
    std::string Language;
    std::optional<std::string> SeriesUtf8;
    std::optional<double> SeriesIndex;
    std::optional<std::string> PublisherUtf8;
    std::optional<int> Year;
    std::optional<std::string> Isbn;
    std::vector<std::string> TagsUtf8;
    std::optional<std::string> DescriptionUtf8;
    std::optional<std::string> Identifier;
    Librova::Domain::EBookFormat Format = Librova::Domain::EBookFormat::Epub;
    std::filesystem::path ManagedPath;
    std::optional<std::filesystem::path> CoverPath;
    std::uintmax_t SizeBytes = 0;
    std::string Sha256Hex;
    std::chrono::system_clock::time_point AddedAtUtc{};
};

class CLibraryCatalogFacade final
{
public:
    CLibraryCatalogFacade(
        const Librova::Domain::IBookQueryRepository& bookQueryRepository,
        const Librova::Domain::IBookRepository& bookRepository);

    [[nodiscard]] SBookListResult ListBooks(const SBookListRequest& request) const;
    [[nodiscard]] std::optional<SBookDetails> GetBookDetails(Librova::Domain::SBookId id) const;
    [[nodiscard]] SLibraryStatistics GetLibraryStatistics() const;

private:
    [[nodiscard]] static Librova::Domain::SSearchQuery ToDomainQuery(const SBookListRequest& request);
    [[nodiscard]] static SBookListItem ToListItem(const Librova::Domain::SBook& book);
    [[nodiscard]] static SBookDetails ToDetails(const Librova::Domain::SBook& book);

    const Librova::Domain::IBookQueryRepository& m_bookQueryRepository;
    const Librova::Domain::IBookRepository& m_bookRepository;
};

} // namespace Librova::Application
