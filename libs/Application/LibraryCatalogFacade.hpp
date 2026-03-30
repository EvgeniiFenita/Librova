#pragma once

#include <chrono>
#include <filesystem>
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

struct SBookListResult
{
    std::vector<SBookListItem> Items;

    [[nodiscard]] bool IsEmpty() const noexcept
    {
        return Items.empty();
    }
};

class CLibraryCatalogFacade final
{
public:
    explicit CLibraryCatalogFacade(const Librova::Domain::IBookQueryRepository& bookQueryRepository);

    [[nodiscard]] SBookListResult ListBooks(const SBookListRequest& request) const;

private:
    [[nodiscard]] static Librova::Domain::SSearchQuery ToDomainQuery(const SBookListRequest& request);
    [[nodiscard]] static SBookListItem ToListItem(const Librova::Domain::SBook& book);

    const Librova::Domain::IBookQueryRepository& m_bookQueryRepository;
};

} // namespace Librova::Application
