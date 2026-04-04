#include "Application/LibraryCatalogFacade.hpp"

#include <stdexcept>

namespace Librova::Application {

CLibraryCatalogFacade::CLibraryCatalogFacade(
    const Librova::Domain::IBookQueryRepository& bookQueryRepository,
    const Librova::Domain::IBookRepository* bookRepository)
    : m_bookQueryRepository(bookQueryRepository)
    , m_bookRepository(bookRepository)
{
}

SBookListResult CLibraryCatalogFacade::ListBooks(const SBookListRequest& request) const
{
    if (!request.IsValid())
    {
        throw std::invalid_argument("Book list request must use a positive limit.");
    }

    const auto domainQuery = ToDomainQuery(request);
    auto languageQuery = domainQuery;
    languageQuery.Language.reset();

    const std::vector<Librova::Domain::SBook> books = m_bookQueryRepository.Search(domainQuery);

    SBookListResult result;
    result.TotalCount = m_bookQueryRepository.CountSearchResults(domainQuery);
    result.AvailableLanguages = m_bookQueryRepository.ListAvailableLanguages(languageQuery);
    result.Items.reserve(books.size());

    for (const Librova::Domain::SBook& book : books)
    {
        result.Items.push_back(ToListItem(book));
    }

    return result;
}

std::optional<SBookDetails> CLibraryCatalogFacade::GetBookDetails(const Librova::Domain::SBookId id) const
{
    if (!id.IsValid())
    {
        throw std::invalid_argument("Book details request must use a valid book id.");
    }

    if (m_bookRepository == nullptr)
    {
        throw std::logic_error("Book details are not available without a book repository.");
    }

    const auto book = m_bookRepository->GetById(id);
    if (!book.has_value())
    {
        return std::nullopt;
    }

    return ToDetails(*book);
}

SLibraryStatistics CLibraryCatalogFacade::GetLibraryStatistics() const
{
    const auto statistics = m_bookQueryRepository.GetLibraryStatistics();
    return {
        .BookCount = statistics.BookCount,
        .TotalManagedBookSizeBytes = statistics.TotalManagedBookSizeBytes,
        .TotalLibrarySizeBytes = statistics.TotalLibrarySizeBytes
    };
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

SBookDetails CLibraryCatalogFacade::ToDetails(const Librova::Domain::SBook& book)
{
    return {
        .Id = book.Id,
        .TitleUtf8 = book.Metadata.TitleUtf8,
        .AuthorsUtf8 = book.Metadata.AuthorsUtf8,
        .Language = book.Metadata.Language,
        .SeriesUtf8 = book.Metadata.SeriesUtf8,
        .SeriesIndex = book.Metadata.SeriesIndex,
        .PublisherUtf8 = book.Metadata.PublisherUtf8,
        .Year = book.Metadata.Year,
        .Isbn = book.Metadata.Isbn,
        .TagsUtf8 = book.Metadata.TagsUtf8,
        .DescriptionUtf8 = book.Metadata.DescriptionUtf8,
        .Identifier = book.Metadata.Identifier,
        .Format = book.File.Format,
        .ManagedPath = book.File.ManagedPath,
        .CoverPath = book.CoverPath,
        .SizeBytes = book.File.SizeBytes,
        .Sha256Hex = book.File.Sha256Hex,
        .AddedAtUtc = book.AddedAtUtc
    };
}

} // namespace Librova::Application
