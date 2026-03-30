#include "Application/LibraryCatalogFacade.hpp"

#include <stdexcept>

namespace Librova::Application {

CLibraryCatalogFacade::CLibraryCatalogFacade(const Librova::Domain::IBookQueryRepository& bookQueryRepository)
    : m_bookQueryRepository(bookQueryRepository)
{
}

SBookListResult CLibraryCatalogFacade::ListBooks(const SBookListRequest& request) const
{
    if (!request.IsValid())
    {
        throw std::invalid_argument("Book list request must use a positive limit.");
    }

    const std::vector<Librova::Domain::SBook> books = m_bookQueryRepository.Search(ToDomainQuery(request));

    SBookListResult result;
    result.Items.reserve(books.size());

    for (const Librova::Domain::SBook& book : books)
    {
        result.Items.push_back(ToListItem(book));
    }

    return result;
}

Librova::Domain::SSearchQuery CLibraryCatalogFacade::ToDomainQuery(const SBookListRequest& request)
{
    return {
        .TextUtf8 = request.TextUtf8,
        .AuthorUtf8 = request.AuthorUtf8,
        .Language = request.Language,
        .SeriesUtf8 = request.SeriesUtf8,
        .TagsUtf8 = request.TagsUtf8,
        .Format = request.Format,
        .SortBy = request.SortBy,
        .Offset = request.Offset,
        .Limit = request.Limit
    };
}

SBookListItem CLibraryCatalogFacade::ToListItem(const Librova::Domain::SBook& book)
{
    return {
        .Id = book.Id,
        .TitleUtf8 = book.Metadata.TitleUtf8,
        .AuthorsUtf8 = book.Metadata.AuthorsUtf8,
        .Language = book.Metadata.Language,
        .SeriesUtf8 = book.Metadata.SeriesUtf8,
        .SeriesIndex = book.Metadata.SeriesIndex,
        .Year = book.Metadata.Year,
        .TagsUtf8 = book.Metadata.TagsUtf8,
        .Format = book.File.Format,
        .ManagedPath = book.File.ManagedPath,
        .CoverPath = book.CoverPath,
        .SizeBytes = book.File.SizeBytes,
        .AddedAtUtc = book.AddedAtUtc
    };
}

} // namespace Librova::Application
