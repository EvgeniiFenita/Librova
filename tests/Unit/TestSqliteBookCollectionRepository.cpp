#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <chrono>
#include <filesystem>

#include "Database/SchemaMigrator.hpp"
#include "Database/SqliteBookCollectionRepository.hpp"
#include "Database/SqliteBookQueryRepository.hpp"
#include "Database/SqliteBookRepository.hpp"
#include "TestWorkspace.hpp"

namespace {

Librova::Domain::SBook MakeBook(
    const std::string_view title,
    const std::string_view hash,
    const std::filesystem::path& managedPath)
{
    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = std::string{title};
    book.Metadata.AuthorsUtf8 = {"Author"};
    book.Metadata.Language = "en";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = managedPath;
    book.File.SizeBytes = 1024;
    book.File.Sha256Hex = std::string{hash};
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::April / 23 / 2026};
    return book;
}

} // namespace

TEST_CASE("Sqlite book collection repository creates lists and deletes collections", "[book-database][collections]")
{
    const auto databasePath = MakeUniqueTestPath(L"librova-book-collections.db");
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    {
        Librova::BookDatabase::CSqliteBookCollectionRepository repository(databasePath);

        const auto first = repository.CreateCollection("Favorites", "star");
        const auto second = repository.CreateCollection("Unread", "bookmark");

        const auto collections = repository.ListCollections();
        REQUIRE(collections.size() == 2);
        REQUIRE(collections[0].NameUtf8 == "Favorites");
        REQUIRE(collections[1].NameUtf8 == "Unread");
        REQUIRE(repository.DeleteCollection(first.Id));
        REQUIRE_FALSE(repository.DeleteCollection(first.Id));

        const auto remaining = repository.ListCollections();
        REQUIRE(remaining == std::vector<Librova::Domain::SBookCollection>{second});

        repository.CloseSession();
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book collection repository orders collection names with Unicode normalization", "[book-database][collections][unicode]")
{
    const auto databasePath = MakeUniqueTestPath(L"librova-book-collections-unicode-order.db");
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    {
        Librova::BookDatabase::CSqliteBookCollectionRepository repository(databasePath);

        static_cast<void>(repository.CreateCollection("Бета", "bookmark"));
        static_cast<void>(repository.CreateCollection("арбуз", "star"));

        const auto collections = repository.ListCollections();
        REQUIRE(collections.size() == 2);
        REQUIRE(collections[0].NameUtf8 == "арбуз");
        REQUIRE(collections[1].NameUtf8 == "Бета");

        repository.CloseSession();
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book collection repository manages book memberships and catalog collection filters", "[book-database][collections]")
{
    const auto databasePath = MakeUniqueTestPath(L"librova-book-collection-membership.db");
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    {
        Librova::BookDatabase::CSqliteBookRepository bookRepository(databasePath);
        Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);
        Librova::BookDatabase::CSqliteBookCollectionRepository collectionRepository(databasePath);

        const auto alphaId = bookRepository.Add(MakeBook("Alpha", "collection-hash-1", "Objects/aa/aa/0000000001.book.epub"));
        const auto betaId = bookRepository.Add(MakeBook("Beta", "collection-hash-2", "Objects/bb/bb/0000000002.book.epub"));
        const auto favorites = collectionRepository.CreateCollection("Favorites", "star");

        REQUIRE(collectionRepository.AddBookToCollection(alphaId, favorites.Id));
        REQUIRE_FALSE(collectionRepository.AddBookToCollection(alphaId, favorites.Id));
        REQUIRE(collectionRepository.ListCollectionsForBook(alphaId) == std::vector<Librova::Domain::SBookCollection>{favorites});
        const auto collectionsByBook = collectionRepository.ListCollectionsForBooks({alphaId, betaId});
        REQUIRE(collectionsByBook.at(alphaId.Value) == std::vector<Librova::Domain::SBookCollection>{favorites});
        REQUIRE_FALSE(collectionsByBook.contains(betaId.Value));

        const auto filteredBooks = queryRepository.Search({
            .CollectionId = favorites.Id,
            .Limit = 10
        });
        REQUIRE(filteredBooks.size() == 1);
        REQUIRE(filteredBooks[0].Metadata.TitleUtf8 == "Alpha");
        REQUIRE(queryRepository.CountSearchResults({
            .CollectionId = favorites.Id,
            .Limit = 10
        }) == 1);

        REQUIRE(collectionRepository.RemoveBookFromCollection(alphaId, favorites.Id));
        REQUIRE_FALSE(collectionRepository.RemoveBookFromCollection(betaId, favorites.Id));
        REQUIRE(collectionRepository.ListCollectionsForBook(alphaId).empty());

        collectionRepository.CloseSession();
        bookRepository.CloseSession();
        queryRepository.CloseSession();
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book collection repository deletes memberships without deleting books", "[book-database][collections]")
{
    const auto databasePath = MakeUniqueTestPath(L"librova-book-collection-delete-preserves-books.db");
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    {
        Librova::BookDatabase::CSqliteBookRepository bookRepository(databasePath);
        Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);
        Librova::BookDatabase::CSqliteBookCollectionRepository collectionRepository(databasePath);

        const auto bookId = bookRepository.Add(MakeBook("Keep Me", "collection-delete-hash", "Objects/cc/cc/0000000003.book.epub"));
        const auto collection = collectionRepository.CreateCollection("Temporary", "folder");
        REQUIRE(collectionRepository.AddBookToCollection(bookId, collection.Id));

        REQUIRE(collectionRepository.DeleteCollection(collection.Id));
        REQUIRE(collectionRepository.ListCollectionsForBook(bookId).empty());

        const auto books = queryRepository.Search({
            .TextUtf8 = "Keep",
            .Limit = 10
        });
        REQUIRE(books.size() == 1);
        REQUIRE(books[0].Id.Value == bookId.Value);

        collectionRepository.CloseSession();
        bookRepository.CloseSession();
        queryRepository.CloseSession();
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book collection repository rejects duplicate collection names", "[book-database][collections]")
{
    const auto databasePath = MakeUniqueTestPath(L"librova-book-collection-duplicate-name.db");
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    {
        Librova::BookDatabase::CSqliteBookCollectionRepository repository(databasePath);

        static_cast<void>(repository.CreateCollection("Favorites", "star"));

        REQUIRE_THROWS_WITH(
            repository.CreateCollection(" favorites ", "bookmark"),
            Catch::Matchers::ContainsSubstring("already exists"));

        repository.CloseSession();
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book collection repository rejects deleting non-deletable collections", "[book-database][collections]")
{
    const auto databasePath = MakeUniqueTestPath(L"librova-book-collection-non-deletable.db");
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    {
        Librova::BookDatabase::CSqliteBookCollectionRepository repository(databasePath);

        const auto collection = repository.CreateCollection(
            "System",
            "crown",
            Librova::Domain::EBookCollectionKind::Preset,
            false);

        REQUIRE_THROWS_WITH(
            repository.DeleteCollection(collection.Id),
            Catch::Matchers::ContainsSubstring("cannot be deleted"));

        repository.CloseSession();
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book collection repository rejects missing book and collection ids", "[book-database][collections]")
{
    const auto databasePath = MakeUniqueTestPath(L"librova-book-collection-missing-ids.db");
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    {
        Librova::BookDatabase::CSqliteBookRepository bookRepository(databasePath);
        Librova::BookDatabase::CSqliteBookCollectionRepository repository(databasePath);

        const auto bookId = bookRepository.Add(MakeBook("Alpha", "collection-hash-missing", "Objects/dd/dd/0000000004.book.epub"));
        const auto collection = repository.CreateCollection("Favorites", "star");

        REQUIRE_THROWS_WITH(
            repository.AddBookToCollection(Librova::Domain::SBookId{9999}, collection.Id),
            Catch::Matchers::ContainsSubstring("Book was not found"));
        REQUIRE_THROWS_WITH(
            repository.AddBookToCollection(bookId, 9999),
            Catch::Matchers::ContainsSubstring("Collection was not found"));

        repository.CloseSession();
        bookRepository.CloseSession();
    }

    std::filesystem::remove(databasePath);
}
