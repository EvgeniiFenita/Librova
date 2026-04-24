#include "App/LibraryCatalogFacade.hpp"

#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace Librova::Application {
namespace {

void EnsureCollectionsAvailable(
    const Librova::Domain::IBookCollectionQueryRepository* queryRepository,
    const Librova::Domain::IBookCollectionRepository* repository)
{
    if (queryRepository == nullptr || repository == nullptr)
    {
        throw std::logic_error("Book collections are not available in this catalog facade.");
    }
}

} // namespace

CLibraryCatalogFacade::CLibraryCatalogFacade(
    const Librova::Domain::IBookQueryRepository& bookQueryRepository,
    const Librova::Domain::IBookRepository& bookRepository)
    : m_bookQueryRepository(bookQueryRepository)
    , m_bookRepository(bookRepository)
{
}

CLibraryCatalogFacade::CLibraryCatalogFacade(
    const Librova::Domain::IBookQueryRepository& bookQueryRepository,
    const Librova::Domain::IBookRepository& bookRepository,
    const Librova::Domain::IBookCollectionQueryRepository& bookCollectionQueryRepository,
    Librova::Domain::IBookCollectionRepository& bookCollectionRepository)
    : m_bookQueryRepository(bookQueryRepository)
    , m_bookRepository(bookRepository)
    , m_bookCollectionQueryRepository(&bookCollectionQueryRepository)
    , m_bookCollectionRepository(&bookCollectionRepository)
{
}

SBookListResult CLibraryCatalogFacade::ListBooks(const SBookListRequest& request) const
{
    if (!request.IsValid())
    {
        throw std::invalid_argument("Book list request must use a positive limit.");
    }

    const auto domainQuery = ToDomainQuery(request);
    auto languageRequest = request;
    languageRequest.Languages.clear();
    auto genreRequest = request;
    genreRequest.GenresUtf8.clear();

    const std::vector<Librova::Domain::SBook> books = m_bookQueryRepository.Search(domainQuery);

    SBookListResult result;
    result.TotalCount = m_bookQueryRepository.CountSearchResults(domainQuery);
    result.AvailableLanguages = m_bookQueryRepository.ListAvailableLanguages(ToDomainQuery(languageRequest));
    result.AvailableGenres = m_bookQueryRepository.ListAvailableGenres(ToDomainQuery(genreRequest));
    result.Statistics = GetLibraryStatistics();
    result.Items.reserve(books.size());

    std::unordered_map<std::int64_t, std::vector<Librova::Domain::SBookCollection>> collectionsByBookId;
    if (m_bookCollectionQueryRepository != nullptr)
    {
        std::vector<Librova::Domain::SBookId> bookIds;
        bookIds.reserve(books.size());
        for (const Librova::Domain::SBook& book : books)
        {
            bookIds.push_back(book.Id);
        }
        collectionsByBookId = m_bookCollectionQueryRepository->ListCollectionsForBooks(bookIds);
    }

    for (const Librova::Domain::SBook& book : books)
    {
        auto item = ToListItem(book);
        if (const auto collections = collectionsByBookId.find(book.Id.Value); collections != collectionsByBookId.end())
        {
            item.Collections = collections->second;
        }
        result.Items.push_back(std::move(item));
    }

    return result;
}

std::optional<SBookDetails> CLibraryCatalogFacade::GetBookDetails(const Librova::Domain::SBookId id) const
{
    if (!id.IsValid())
    {
        throw std::invalid_argument("Book details request must use a valid book id.");
    }

    const auto book = m_bookRepository.GetById(id);
    if (!book.has_value())
    {
        return std::nullopt;
    }

    auto details = ToDetails(*book);
    if (m_bookCollectionQueryRepository != nullptr)
    {
        details.Collections = m_bookCollectionQueryRepository->ListCollectionsForBook(id);
    }

    return details;
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

std::vector<Librova::Domain::SBookCollection> CLibraryCatalogFacade::ListCollections() const
{
    EnsureCollectionsAvailable(m_bookCollectionQueryRepository, m_bookCollectionRepository);
    return m_bookCollectionQueryRepository->ListCollections();
}

Librova::Domain::SBookCollection CLibraryCatalogFacade::CreateCollection(
    std::string nameUtf8,
    std::string iconKey) const
{
    EnsureCollectionsAvailable(m_bookCollectionQueryRepository, m_bookCollectionRepository);
    return m_bookCollectionRepository->CreateCollection(std::move(nameUtf8), std::move(iconKey));
}

bool CLibraryCatalogFacade::DeleteCollection(const std::int64_t collectionId) const
{
    EnsureCollectionsAvailable(m_bookCollectionQueryRepository, m_bookCollectionRepository);
    return m_bookCollectionRepository->DeleteCollection(collectionId);
}

bool CLibraryCatalogFacade::AddBookToCollection(const Librova::Domain::SBookId bookId, const std::int64_t collectionId) const
{
    EnsureCollectionsAvailable(m_bookCollectionQueryRepository, m_bookCollectionRepository);
    return m_bookCollectionRepository->AddBookToCollection(bookId, collectionId);
}

bool CLibraryCatalogFacade::RemoveBookFromCollection(const Librova::Domain::SBookId bookId, const std::int64_t collectionId) const
{
    EnsureCollectionsAvailable(m_bookCollectionQueryRepository, m_bookCollectionRepository);
    return m_bookCollectionRepository->RemoveBookFromCollection(bookId, collectionId);
}

Librova::Domain::SSearchQuery CLibraryCatalogFacade::ToDomainQuery(const SBookListRequest& request)
{
    return {
        .TextUtf8 = request.TextUtf8,
        .AuthorUtf8 = request.AuthorUtf8,
        .Languages = request.Languages,
        .CollectionId = request.CollectionId,
        .SeriesUtf8 = request.SeriesUtf8,
        .TagsUtf8 = request.TagsUtf8,
        .GenresUtf8 = request.GenresUtf8,
        .Format = request.Format,
        .SortBy = request.SortBy,
        .SortDirection = request.SortDirection,
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
        .GenresUtf8 = book.Metadata.GenresUtf8,
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
        .GenresUtf8 = book.Metadata.GenresUtf8,
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
