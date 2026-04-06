#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>
#include <unordered_set>

#include "BookDatabase/SqliteBookQueryRepository.hpp"
#include "BookDatabase/SqliteBookRepository.hpp"
#include "DatabaseRuntime/SchemaMigrator.hpp"
#include "Sqlite/SqliteConnection.hpp"
#include "Sqlite/SqliteStatement.hpp"

TEST_CASE("Sqlite book repository round-trips book metadata and files", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-repository.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Пикник на обочине";
    book.Metadata.AuthorsUtf8 = {"Аркадий Стругацкий", "Борис Стругацкий"};
    book.Metadata.Language = "ru";
    book.Metadata.SeriesUtf8 = std::string{"Миры"};
    book.Metadata.SeriesIndex = 1.5;
    book.Metadata.PublisherUtf8 = std::string{"АСТ"};
    book.Metadata.Year = 1972;
    book.Metadata.Isbn = std::string{"978-5-17-090334-4"};
    book.Metadata.TagsUtf8 = {"sci-fi", "classic"};
    book.Metadata.DescriptionUtf8 = std::string{"Классика советской фантастики"};
    book.Metadata.Identifier = std::string{"roadside-picnic"};
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.StorageEncoding = Librova::Domain::EStorageEncoding::Compressed;
    book.File.ManagedPath = std::filesystem::path(u8"Books/0000000001/Пикник.epub");
    book.File.SizeBytes = 4096;
    book.File.Sha256Hex = "abc123";
    book.CoverPath = std::filesystem::path(u8"Covers/0000000001.jpg");
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{14} + std::chrono::minutes{5};

    const Librova::Domain::SBookId bookId = repository.Add(book);
    REQUIRE(bookId.IsValid());

    const std::optional<Librova::Domain::SBook> loadedBook = repository.GetById(bookId);

    REQUIRE(loadedBook.has_value());
    REQUIRE(loadedBook->Id.Value == bookId.Value);
    REQUIRE(loadedBook->Metadata.TitleUtf8 == book.Metadata.TitleUtf8);
    REQUIRE(loadedBook->Metadata.AuthorsUtf8 == book.Metadata.AuthorsUtf8);
    REQUIRE(loadedBook->Metadata.Language == book.Metadata.Language);
    REQUIRE(loadedBook->Metadata.SeriesUtf8 == book.Metadata.SeriesUtf8);
    REQUIRE(loadedBook->Metadata.SeriesIndex == book.Metadata.SeriesIndex);
    REQUIRE(loadedBook->Metadata.PublisherUtf8 == book.Metadata.PublisherUtf8);
    REQUIRE(loadedBook->Metadata.Year == book.Metadata.Year);
    REQUIRE(loadedBook->Metadata.Isbn == std::optional<std::string>{"9785170903344"});
    REQUIRE(loadedBook->Metadata.TagsUtf8 == std::vector<std::string>({"classic", "sci-fi"}));
    REQUIRE(loadedBook->Metadata.DescriptionUtf8 == book.Metadata.DescriptionUtf8);
    REQUIRE(loadedBook->Metadata.Identifier == book.Metadata.Identifier);
    REQUIRE(loadedBook->File.Format == book.File.Format);
    REQUIRE(loadedBook->File.StorageEncoding == book.File.StorageEncoding);
    REQUIRE(loadedBook->File.ManagedPath == book.File.ManagedPath);
    REQUIRE(loadedBook->File.SizeBytes == book.File.SizeBytes);
    REQUIRE(loadedBook->File.Sha256Hex == book.File.Sha256Hex);
    REQUIRE(loadedBook->CoverPath == book.CoverPath);
    REQUIRE(loadedBook->AddedAtUtc == book.AddedAtUtc);

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);
        Librova::Sqlite::CSqliteStatement authorStatement(
            connection.GetNativeHandle(),
            "SELECT COUNT(*) FROM authors;");
        REQUIRE(authorStatement.Step());
        REQUIRE(authorStatement.GetColumnInt(0) == 2);

        Librova::Sqlite::CSqliteStatement tagStatement(
            connection.GetNativeHandle(),
            "SELECT COUNT(*) FROM tags;");
        REQUIRE(tagStatement.Step());
        REQUIRE(tagStatement.GetColumnInt(0) == 2);
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book repository reserves sequential book ids", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-repository-reserve.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    Librova::BookDatabase::CSqliteBookRepository secondRepository(databasePath);
    const Librova::Domain::SBookId firstReservedId = repository.ReserveId();
    REQUIRE(firstReservedId.IsValid());
    REQUIRE(firstReservedId.Value == 1);

    const Librova::Domain::SBookId secondReservedId = secondRepository.ReserveId();
    REQUIRE(secondReservedId.Value == firstReservedId.Value + 1);

    Librova::Domain::SBook book;
    book.Id = firstReservedId;
    book.Metadata.TitleUtf8 = "Reserved Id";
    book.Metadata.AuthorsUtf8 = {"Author"};
    book.Metadata.Language = "en";
    book.File.ManagedPath = "Books/0000000001/book.epub";
    book.File.SizeBytes = 100;
    book.File.Sha256Hex = "reserve-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    REQUIRE(repository.Add(book).Value == firstReservedId.Value);

    const Librova::Domain::SBookId thirdReservedId = repository.ReserveId();
    REQUIRE(thirdReservedId.Value == secondReservedId.Value + 1);

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book repository removes stored books", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-repository-remove.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Roadside Picnic";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.File.ManagedPath = "Books/0000000002/book.epub";
    book.File.SizeBytes = 1024;
    book.File.Sha256Hex = "deadbeef";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};

    const Librova::Domain::SBookId bookId = repository.Add(book);
    REQUIRE(repository.GetById(bookId).has_value());

    repository.Remove(bookId);

    REQUIRE_FALSE(repository.GetById(bookId).has_value());

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);

        Librova::Sqlite::CSqliteStatement authorLinkStatement(
            connection.GetNativeHandle(),
            "SELECT COUNT(*) FROM book_authors WHERE book_id = ?;");
        authorLinkStatement.BindInt64(1, bookId.Value);
        REQUIRE(authorLinkStatement.Step());
        REQUIRE(authorLinkStatement.GetColumnInt(0) == 0);

        Librova::Sqlite::CSqliteStatement formatStatement(
            connection.GetNativeHandle(),
            "SELECT COUNT(*) FROM formats WHERE book_id = ?;");
        formatStatement.BindInt64(1, bookId.Value);
        REQUIRE(formatStatement.Step());
        REQUIRE(formatStatement.GetColumnInt(0) == 0);

        Librova::Sqlite::CSqliteStatement searchIndexStatement(
            connection.GetNativeHandle(),
            "SELECT COUNT(*) FROM search_index WHERE rowid = ?;");
        searchIndexStatement.BindInt64(1, bookId.Value);
        REQUIRE(searchIndexStatement.Step());
        REQUIRE(searchIndexStatement.GetColumnInt(0) == 0);
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book repository inserts exactly one search-index row per added book", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-repository-search-index.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Roadside Picnic";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.Metadata.DescriptionUtf8 = std::string{"Zone"};
    book.File.ManagedPath = "Books/0000000003/book.epub";
    book.File.SizeBytes = 1024;
    book.File.Sha256Hex = "search-index";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};

    const auto bookId = repository.Add(book);

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);
        Librova::Sqlite::CSqliteStatement statement(
            connection.GetNativeHandle(),
            "SELECT COUNT(*) FROM search_index WHERE rowid = ?;");
        statement.BindInt64(1, bookId.Value);
        REQUIRE(statement.Step());
        REQUIRE(statement.GetColumnInt(0) == 1);
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book query repository applies structured filters and FTS text search", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-query-search.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook firstBook;
    firstBook.Metadata.TitleUtf8 = "Пикник на обочине";
    firstBook.Metadata.AuthorsUtf8 = {"Аркадий Стругацкий", "Борис Стругацкий"};
    firstBook.Metadata.Language = "ru";
    firstBook.Metadata.TagsUtf8 = {"classic", "sci-fi"};
    firstBook.File.Format = Librova::Domain::EBookFormat::Epub;
    firstBook.File.ManagedPath = "Books/0000000101/book.epub";
    firstBook.File.SizeBytes = 2048;
    firstBook.File.Sha256Hex = "hash-1";
    firstBook.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    static_cast<void>(writeRepository.Add(firstBook));

    Librova::Domain::SBook secondBook;
    secondBook.Metadata.TitleUtf8 = "Roadside Picnic";
    secondBook.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    secondBook.Metadata.Language = "en";
    secondBook.Metadata.TagsUtf8 = {"sci-fi"};
    secondBook.Metadata.DescriptionUtf8 = std::string{"A zone, artifacts, and dangerous expeditions"};
    secondBook.File.Format = Librova::Domain::EBookFormat::Fb2;
    secondBook.File.ManagedPath = "Books/0000000102/book.fb2";
    secondBook.File.SizeBytes = 1024;
    secondBook.File.Sha256Hex = "hash-2";
    secondBook.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{1};
    static_cast<void>(writeRepository.Add(secondBook));

    const Librova::Domain::SSearchQuery query{
        .AuthorUtf8 = std::string{"Борис Стругацкий"},
        .Language = std::string{"ru"},
        .TagsUtf8 = {"classic"},
        .Format = Librova::Domain::EBookFormat::Epub,
        .SortBy = Librova::Domain::EBookSort::Title
    };

    const std::vector<Librova::Domain::SBook> books = queryRepository.Search(query);

    REQUIRE(books.size() == 1);
    REQUIRE(books.front().Metadata.TitleUtf8 == "Пикник на обочине");

    const std::vector<Librova::Domain::SBook> authorSearch = queryRepository.Search({
        .TextUtf8 = "arkady"
    });
    REQUIRE(authorSearch.size() == 1);
    REQUIRE(authorSearch.front().Metadata.TitleUtf8 == "Roadside Picnic");

    const std::vector<Librova::Domain::SBook> tagSearch = queryRepository.Search({
        .TextUtf8 = "classic"
    });
    REQUIRE(tagSearch.size() == 1);
    REQUIRE(tagSearch.front().Metadata.TitleUtf8 == "Пикник на обочине");

    const std::vector<Librova::Domain::SBook> descriptionSearch = queryRepository.Search({
        .TextUtf8 = "artifacts"
    });
    REQUIRE(descriptionSearch.size() == 1);
    REQUIRE(descriptionSearch.front().Metadata.TitleUtf8 == "Roadside Picnic");

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book query repository lists available languages with combined filters even when current language is set", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-query-language-filters.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook firstBook;
    firstBook.Metadata.TitleUtf8 = "Shared Shelf";
    firstBook.Metadata.AuthorsUtf8 = {"Common Author"};
    firstBook.Metadata.Language = "en";
    firstBook.Metadata.SeriesUtf8 = std::string{"Series Shared"};
    firstBook.Metadata.TagsUtf8 = {"shared-tag"};
    firstBook.File.Format = Librova::Domain::EBookFormat::Epub;
    firstBook.File.ManagedPath = "Books/0000000103/first.epub";
    firstBook.File.SizeBytes = 100;
    firstBook.File.Sha256Hex = "lang-filter-1";
    firstBook.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    static_cast<void>(writeRepository.Add(firstBook));

    Librova::Domain::SBook secondBook = firstBook;
    secondBook.Metadata.Language = "ru";
    secondBook.File.ManagedPath = "Books/0000000104/second.epub";
    secondBook.File.Sha256Hex = "lang-filter-2";
    secondBook.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{1};
    static_cast<void>(writeRepository.Add(secondBook));

    Librova::Domain::SBook ignoredBook = firstBook;
    ignoredBook.Metadata.Language = "de";
    ignoredBook.Metadata.SeriesUtf8 = std::string{"Different Series"};
    ignoredBook.File.ManagedPath = "Books/0000000105/ignored.epub";
    ignoredBook.File.Sha256Hex = "lang-filter-3";
    ignoredBook.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{2};
    static_cast<void>(writeRepository.Add(ignoredBook));

    const std::vector<std::string> languages = queryRepository.ListAvailableLanguages({
        .TextUtf8 = "shared",
        .AuthorUtf8 = std::string{"Common Author"},
        .Language = std::string{"en"},
        .SeriesUtf8 = std::string{"Series Shared"},
        .TagsUtf8 = {"shared-tag"},
        .Format = Librova::Domain::EBookFormat::Epub
    });

    REQUIRE(languages == std::vector<std::string>({"en", "ru"}));

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book query repository supports Cyrillic prefix search and е ё equivalence", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-query-cyrillic.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook firstBook;
    firstBook.Metadata.TitleUtf8 = "Ёжик в тумане";
    firstBook.Metadata.AuthorsUtf8 = {"Юрий Норштейн"};
    firstBook.Metadata.Language = "ru";
    firstBook.Metadata.DescriptionUtf8 = std::string{"История о ежике и тумане"};
    firstBook.File.Format = Librova::Domain::EBookFormat::Epub;
    firstBook.File.ManagedPath = "Books/0000000401/book.epub";
    firstBook.File.SizeBytes = 333;
    firstBook.File.Sha256Hex = "hash-cyr-1";
    firstBook.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    static_cast<void>(writeRepository.Add(firstBook));

    Librova::Domain::SBook secondBook;
    secondBook.Metadata.TitleUtf8 = "Пикник на обочине";
    secondBook.Metadata.AuthorsUtf8 = {"Аркадий Стругацкий", "Борис Стругацкий"};
    secondBook.Metadata.Language = "ru";
    secondBook.Metadata.DescriptionUtf8 = std::string{"Зона и опасные экспедиции"};
    secondBook.File.Format = Librova::Domain::EBookFormat::Fb2;
    secondBook.File.ManagedPath = "Books/0000000402/book.fb2";
    secondBook.File.SizeBytes = 444;
    secondBook.File.Sha256Hex = "hash-cyr-2";
    secondBook.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{1};
    static_cast<void>(writeRepository.Add(secondBook));

    const std::vector<Librova::Domain::SBook> prefixSearch = queryRepository.Search({
        .TextUtf8 = "пик"
    });
    REQUIRE(prefixSearch.size() == 1);
    REQUIRE(prefixSearch.front().Metadata.TitleUtf8 == "Пикник на обочине");

    const std::vector<Librova::Domain::SBook> yoSearch = queryRepository.Search({
        .TextUtf8 = "ежик"
    });
    REQUIRE(yoSearch.size() == 1);
    REQUIRE(yoSearch.front().Metadata.TitleUtf8 == "Ёжик в тумане");

    const std::vector<Librova::Domain::SBook> upperCaseSearch = queryRepository.Search({
        .TextUtf8 = "БОРИС"
    });
    REQUIRE(upperCaseSearch.size() == 1);
    REQUIRE(upperCaseSearch.front().Metadata.TitleUtf8 == "Пикник на обочине");

    const std::vector<Librova::Domain::SBook> descriptionPrefixSearch = queryRepository.Search({
        .TextUtf8 = "экспед"
    });
    REQUIRE(descriptionPrefixSearch.size() == 1);
    REQUIRE(descriptionPrefixSearch.front().Metadata.TitleUtf8 == "Пикник на обочине");

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book query repository keeps extended Cyrillic search case-insensitive", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-query-extended-cyrillic.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Прыгоды ў горадзе";
    book.Metadata.AuthorsUtf8 = {"Іван Єнін", "Їжак Аўтар"};
    book.Metadata.Language = "be";
    book.Metadata.DescriptionUtf8 = std::string{"Кніга пра Івана, їжака і вясёлыя падзеі"};
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Books/0000000403/book.epub";
    book.File.SizeBytes = 555;
    book.File.Sha256Hex = "hash-cyr-3";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{2};
    static_cast<void>(writeRepository.Add(book));

    const std::vector<Librova::Domain::SBook> titleSearch = queryRepository.Search({
        .TextUtf8 = "ПРЫГОДЫ Ў"
    });
    REQUIRE(titleSearch.size() == 1);
    REQUIRE(titleSearch.front().Metadata.TitleUtf8 == "Прыгоды ў горадзе");

    const std::vector<Librova::Domain::SBook> authorSearch = queryRepository.Search({
        .TextUtf8 = "ІВАН"
    });
    REQUIRE(authorSearch.size() == 1);
    REQUIRE(authorSearch.front().Metadata.TitleUtf8 == "Прыгоды ў горадзе");

    const std::vector<Librova::Domain::SBook> descriptionSearch = queryRepository.Search({
        .TextUtf8 = "ЇЖАК"
    });
    REQUIRE(descriptionSearch.size() == 1);
    REQUIRE(descriptionSearch.front().Metadata.TitleUtf8 == "Прыгоды ў горадзе");

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book query repository ignores FTS syntax punctuation in free-text search", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-query-fts-syntax.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Roadside Picnic";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.Metadata.DescriptionUtf8 = std::string{"A zone full of artifacts and dangerous expeditions"};
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Books/0000000501/book.epub";
    book.File.SizeBytes = 512;
    book.File.Sha256Hex = "hash-fts-syntax";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    static_cast<void>(writeRepository.Add(book));

    const std::vector<Librova::Domain::SBook> books = queryRepository.Search({
        .TextUtf8 = R"(roadside: ("zone") -artifacts)"
    });

    REQUIRE(books.size() == 1);
    REQUIRE(books.front().Metadata.TitleUtf8 == "Roadside Picnic");

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book query repository hydrates multi-book search results without losing order or metadata", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-query-bulk-hydration.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook firstBook;
    firstBook.Metadata.TitleUtf8 = "Alpha Title";
    firstBook.Metadata.AuthorsUtf8 = {"Primary Author", "Second Author"};
    firstBook.Metadata.Language = "en";
    firstBook.Metadata.TagsUtf8 = {"tag-b", "tag-a"};
    firstBook.Metadata.SeriesUtf8 = std::string{"Series A"};
    firstBook.Metadata.SeriesIndex = 2.0;
    firstBook.Metadata.DescriptionUtf8 = std::string{"First description"};
    firstBook.File.Format = Librova::Domain::EBookFormat::Epub;
    firstBook.File.ManagedPath = std::filesystem::path(u8"Books/0000000601/alpha.epub");
    firstBook.File.SizeBytes = 600;
    firstBook.File.Sha256Hex = "hash-bulk-1";
    firstBook.CoverPath = std::filesystem::path(u8"Covers/0000000601.jpg");
    firstBook.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    static_cast<void>(writeRepository.Add(firstBook));

    Librova::Domain::SBook secondBook;
    secondBook.Metadata.TitleUtf8 = "Beta Title";
    secondBook.Metadata.AuthorsUtf8 = {"Solo Author"};
    secondBook.Metadata.Language = "en";
    secondBook.Metadata.TagsUtf8 = {"tag-c"};
    secondBook.Metadata.DescriptionUtf8 = std::string{"Second description"};
    secondBook.File.Format = Librova::Domain::EBookFormat::Fb2;
    secondBook.File.ManagedPath = std::filesystem::path(u8"Books/0000000602/beta.fb2");
    secondBook.File.SizeBytes = 700;
    secondBook.File.Sha256Hex = "hash-bulk-2";
    secondBook.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{1};
    static_cast<void>(writeRepository.Add(secondBook));

    const std::vector<Librova::Domain::SBook> books = queryRepository.Search({
        .Language = std::string{"en"},
        .SortBy = Librova::Domain::EBookSort::Title
    });

    REQUIRE(books.size() == 2);
    REQUIRE(books[0].Metadata.TitleUtf8 == "Alpha Title");
    REQUIRE(books[0].Metadata.AuthorsUtf8 == std::vector<std::string>({"Primary Author", "Second Author"}));
    REQUIRE(books[0].Metadata.TagsUtf8 == std::vector<std::string>({"tag-a", "tag-b"}));
    REQUIRE(books[0].Metadata.SeriesUtf8 == std::optional<std::string>{"Series A"});
    REQUIRE(books[0].Metadata.SeriesIndex == std::optional<double>{2.0});
    REQUIRE(books[0].Metadata.DescriptionUtf8 == std::optional<std::string>{"First description"});
    REQUIRE(books[0].CoverPath == std::optional<std::filesystem::path>{std::filesystem::path(u8"Covers/0000000601.jpg")});
    REQUIRE(books[0].File.ManagedPath == std::filesystem::path(u8"Books/0000000601/alpha.epub"));

    REQUIRE(books[1].Metadata.TitleUtf8 == "Beta Title");
    REQUIRE(books[1].Metadata.AuthorsUtf8 == std::vector<std::string>({"Solo Author"}));
    REQUIRE(books[1].Metadata.TagsUtf8 == std::vector<std::string>({"tag-c"}));
    REQUIRE(books[1].Metadata.DescriptionUtf8 == std::optional<std::string>{"Second description"});
    REQUIRE(books[1].File.ManagedPath == std::filesystem::path(u8"Books/0000000602/beta.fb2"));
    REQUIRE_FALSE(books[1].CoverPath.has_value());

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book query repository classifies strict and probable duplicates", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-query-duplicates.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Ёжик в тумане";
    book.Metadata.AuthorsUtf8 = {"Юрий Норштейн", "Франческа Ярбусова"};
    book.Metadata.Language = "ru";
    book.Metadata.Isbn = std::string{"978-5-17-090334-4"};
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Books/0000000201/book.epub";
    book.File.SizeBytes = 512;
    book.File.Sha256Hex = "same-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};

    const Librova::Domain::SBookId storedId = writeRepository.Add(book);

    const Librova::Domain::SCandidateBook strictCandidate{
        .Metadata = {
            .TitleUtf8 = "Different Title",
            .AuthorsUtf8 = {"Another Author"},
            .Language = "ru",
            .Isbn = std::string{"9785170903344"}
        },
        .Format = Librova::Domain::EBookFormat::Epub,
        .Sha256Hex = std::string{"same-hash"}
    };

    const std::vector<Librova::Domain::SDuplicateMatch> strictMatches = queryRepository.FindDuplicates(strictCandidate);

    REQUIRE(strictMatches.size() == 1);
    REQUIRE(strictMatches.front().Severity == Librova::Domain::EDuplicateSeverity::Strict);
    REQUIRE(strictMatches.front().ExistingBookId.Value == storedId.Value);

    const Librova::Domain::SCandidateBook probableCandidate{
        .Metadata = {
            .TitleUtf8 = "Ежик   в тумане",
            .AuthorsUtf8 = {"Франческа Ярбусова", "Юрий Норштейн"},
            .Language = "ru"
        },
        .Format = Librova::Domain::EBookFormat::Epub
    };

    const std::vector<Librova::Domain::SDuplicateMatch> probableMatches = queryRepository.FindDuplicates(probableCandidate);

    REQUIRE(probableMatches.size() == 1);
    REQUIRE(probableMatches.front().Severity == Librova::Domain::EDuplicateSeverity::Probable);
    REQUIRE(probableMatches.front().Reason == Librova::Domain::EDuplicateReason::SameNormalizedTitleAndAuthors);
    REQUIRE(probableMatches.front().ExistingBookId.Value == storedId.Value);

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book query repository probable duplicate detection requires the full normalized author set", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-query-probable-author-set.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook matchingBook;
    matchingBook.Metadata.TitleUtf8 = "Roadside Picnic";
    matchingBook.Metadata.AuthorsUtf8 = {"Arkady Strugatsky", "Boris Strugatsky"};
    matchingBook.Metadata.Language = "ru";
    matchingBook.File.Format = Librova::Domain::EBookFormat::Epub;
    matchingBook.File.ManagedPath = "Books/0000000701/match.epub";
    matchingBook.File.SizeBytes = 800;
    matchingBook.File.Sha256Hex = "hash-probable-1";
    matchingBook.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};

    const auto matchingBookId = writeRepository.Add(matchingBook);

    Librova::Domain::SBook extraAuthorBook;
    extraAuthorBook.Metadata.TitleUtf8 = "Roadside Picnic";
    extraAuthorBook.Metadata.AuthorsUtf8 = {"Arkady Strugatsky", "Boris Strugatsky", "Translator Name"};
    extraAuthorBook.Metadata.Language = "ru";
    extraAuthorBook.File.Format = Librova::Domain::EBookFormat::Epub;
    extraAuthorBook.File.ManagedPath = "Books/0000000702/extra.epub";
    extraAuthorBook.File.SizeBytes = 900;
    extraAuthorBook.File.Sha256Hex = "hash-probable-2";
    extraAuthorBook.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{1};
    static_cast<void>(writeRepository.Add(extraAuthorBook));

    const std::vector<Librova::Domain::SDuplicateMatch> probableMatches = queryRepository.FindDuplicates({
        .Metadata = {
            .TitleUtf8 = "  roadside   picnic ",
            .AuthorsUtf8 = {"BORIS STRUGATSKY", "arkady strugatsky"},
            .Language = "ru"
        },
        .Format = Librova::Domain::EBookFormat::Epub
    });

    REQUIRE(probableMatches.size() == 1);
    REQUIRE(probableMatches.front().ExistingBookId.Value == matchingBookId.Value);
    REQUIRE(probableMatches.front().Reason == Librova::Domain::EDuplicateReason::SameNormalizedTitleAndAuthors);

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book query repository probable duplicate detection requires the normalized title as well as the author set", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-query-probable-title-match.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook storedBook;
    storedBook.Metadata.TitleUtf8 = "Definitely Maybe";
    storedBook.Metadata.AuthorsUtf8 = {"Arkady Strugatsky", "Boris Strugatsky"};
    storedBook.Metadata.Language = "ru";
    storedBook.File.Format = Librova::Domain::EBookFormat::Epub;
    storedBook.File.ManagedPath = "Books/0000000707/maybe.epub";
    storedBook.File.SizeBytes = 902;
    storedBook.File.Sha256Hex = "hash-probable-7";
    storedBook.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{6};
    static_cast<void>(writeRepository.Add(storedBook));

    const std::vector<Librova::Domain::SDuplicateMatch> probableMatches = queryRepository.FindDuplicates({
        .Metadata = {
            .TitleUtf8 = "Roadside Picnic",
            .AuthorsUtf8 = {"Boris Strugatsky", "Arkady Strugatsky"},
            .Language = "ru"
        },
        .Format = Librova::Domain::EBookFormat::Epub
    });

    REQUIRE(probableMatches.empty());

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book query repository probable duplicate detection keeps duplicate-author candidate semantics unchanged", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-query-duplicate-author-candidate.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook storedBook;
    storedBook.Metadata.TitleUtf8 = "Solaris";
    storedBook.Metadata.AuthorsUtf8 = {"Stanislaw Lem"};
    storedBook.Metadata.Language = "pl";
    storedBook.File.Format = Librova::Domain::EBookFormat::Epub;
    storedBook.File.ManagedPath = "Books/0000000704/solaris.epub";
    storedBook.File.SizeBytes = 901;
    storedBook.File.Sha256Hex = "hash-probable-4";
    storedBook.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{3};
    static_cast<void>(writeRepository.Add(storedBook));

    const std::vector<Librova::Domain::SDuplicateMatch> probableMatches = queryRepository.FindDuplicates({
        .Metadata = {
            .TitleUtf8 = "solaris",
            .AuthorsUtf8 = {"Stanislaw Lem", " stanislaw   lem "},
            .Language = "pl"
        },
        .Format = Librova::Domain::EBookFormat::Epub
    });

    REQUIRE(probableMatches.empty());

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book query repository returns all probable duplicate matches for the same normalized title and authors", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-query-multiple-probable-duplicates.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook firstBook;
    firstBook.Metadata.TitleUtf8 = "Hard to Be a God";
    firstBook.Metadata.AuthorsUtf8 = {"Arkady Strugatsky", "Boris Strugatsky"};
    firstBook.Metadata.Language = "ru";
    firstBook.File.Format = Librova::Domain::EBookFormat::Epub;
    firstBook.File.ManagedPath = "Books/0000000705/first.epub";
    firstBook.File.SizeBytes = 700;
    firstBook.File.Sha256Hex = "hash-probable-5";
    firstBook.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{4};
    const auto firstBookId = writeRepository.Add(firstBook);

    Librova::Domain::SBook secondBook = firstBook;
    secondBook.File.ManagedPath = "Books/0000000706/second.epub";
    secondBook.File.Sha256Hex = "hash-probable-6";
    secondBook.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{5};
    const auto secondBookId = writeRepository.Add(secondBook);

    const std::vector<Librova::Domain::SDuplicateMatch> probableMatches = queryRepository.FindDuplicates({
        .Metadata = {
            .TitleUtf8 = " hard   to be  a god ",
            .AuthorsUtf8 = {"BORIS STRUGATSKY", "arkady strugatsky"},
            .Language = "ru"
        },
        .Format = Librova::Domain::EBookFormat::Epub
    });

    REQUIRE(probableMatches.size() == 2);
    REQUIRE(probableMatches[0].Severity == Librova::Domain::EDuplicateSeverity::Probable);
    REQUIRE(probableMatches[0].Reason == Librova::Domain::EDuplicateReason::SameNormalizedTitleAndAuthors);
    REQUIRE(probableMatches[1].Severity == Librova::Domain::EDuplicateSeverity::Probable);
    REQUIRE(probableMatches[1].Reason == Librova::Domain::EDuplicateReason::SameNormalizedTitleAndAuthors);

    const std::unordered_set<std::int64_t> duplicateIds{
        probableMatches[0].ExistingBookId.Value,
        probableMatches[1].ExistingBookId.Value
    };
    REQUIRE(duplicateIds == std::unordered_set<std::int64_t>{firstBookId.Value, secondBookId.Value});

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book query repository probable duplicate detection stays case-insensitive for extended Cyrillic", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-query-extended-cyrillic-duplicates.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook storedBook;
    storedBook.Metadata.TitleUtf8 = "Прыгоды ў горадзе";
    storedBook.Metadata.AuthorsUtf8 = {"Іван Єнін", "Їжак Аўтар"};
    storedBook.Metadata.Language = "be";
    storedBook.File.Format = Librova::Domain::EBookFormat::Epub;
    storedBook.File.ManagedPath = "Books/0000000703/match.epub";
    storedBook.File.SizeBytes = 901;
    storedBook.File.Sha256Hex = "hash-probable-3";
    storedBook.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{2};

    const auto storedBookId = writeRepository.Add(storedBook);

    const std::vector<Librova::Domain::SDuplicateMatch> probableMatches = queryRepository.FindDuplicates({
        .Metadata = {
            .TitleUtf8 = "ПРЫГОДЫ Ў ГОРАДЗЕ",
            .AuthorsUtf8 = {"їжак аўтар", "іван єнін"},
            .Language = "be"
        },
        .Format = Librova::Domain::EBookFormat::Epub
    });

    REQUIRE(probableMatches.size() == 1);
    REQUIRE(probableMatches.front().ExistingBookId.Value == storedBookId.Value);
    REQUIRE(probableMatches.front().Reason == Librova::Domain::EDuplicateReason::SameNormalizedTitleAndAuthors);

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book repository tolerates duplicate authors and tags after normalization", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-repository-dedup.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Duplicate Normalization";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky", " arkady   strugatsky ", "ARKADY STRUGATSKY"};
    book.Metadata.Language = "en";
    book.Metadata.TagsUtf8 = {"Sci-Fi", " sci-fi ", "SCI-FI"};
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Books/0000000301/book.epub";
    book.File.SizeBytes = 256;
    book.File.Sha256Hex = "hash-dedup";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};

    const Librova::Domain::SBookId bookId = repository.Add(book);
    const std::optional<Librova::Domain::SBook> loadedBook = repository.GetById(bookId);

    REQUIRE(loadedBook.has_value());
    REQUIRE(loadedBook->Metadata.AuthorsUtf8 == std::vector<std::string>({"Arkady Strugatsky"}));
    REQUIRE(loadedBook->Metadata.TagsUtf8 == std::vector<std::string>({"Sci-Fi"}));

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);

        Librova::Sqlite::CSqliteStatement authorLinkStatement(
            connection.GetNativeHandle(),
            "SELECT COUNT(*) FROM book_authors WHERE book_id = ?;");
        authorLinkStatement.BindInt64(1, bookId.Value);
        REQUIRE(authorLinkStatement.Step());
        REQUIRE(authorLinkStatement.GetColumnInt(0) == 1);

        Librova::Sqlite::CSqliteStatement tagLinkStatement(
            connection.GetNativeHandle(),
            "SELECT COUNT(*) FROM book_tags WHERE book_id = ?;");
        tagLinkStatement.BindInt64(1, bookId.Value);
        REQUIRE(tagLinkStatement.Step());
        REQUIRE(tagLinkStatement.GetColumnInt(0) == 1);
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book repository Add throws CDuplicateHashException when sha256_hex already exists", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-repository-hash-conflict.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);

    Librova::Domain::SBook firstBook;
    firstBook.Metadata.TitleUtf8 = "First Book";
    firstBook.Metadata.AuthorsUtf8 = {"Author A"};
    firstBook.Metadata.Language = "en";
    firstBook.File.Format = Librova::Domain::EBookFormat::Fb2;
    firstBook.File.StorageEncoding = Librova::Domain::EStorageEncoding::Plain;
    firstBook.File.ManagedPath = std::filesystem::path{"Books/0000000001/first.fb2"};
    firstBook.File.SizeBytes = 1024;
    firstBook.File.Sha256Hex = "aabbccddeeff";
    firstBook.AddedAtUtc = std::chrono::system_clock::now();

    const Librova::Domain::SBookId firstId = repository.Add(firstBook);
    REQUIRE(firstId.IsValid());

    Librova::Domain::SBook secondBook;
    secondBook.Metadata.TitleUtf8 = "Second Book";
    secondBook.Metadata.AuthorsUtf8 = {"Author B"};
    secondBook.Metadata.Language = "en";
    secondBook.File.Format = Librova::Domain::EBookFormat::Fb2;
    secondBook.File.StorageEncoding = Librova::Domain::EStorageEncoding::Plain;
    secondBook.File.ManagedPath = std::filesystem::path{"Books/0000000002/second.fb2"};
    secondBook.File.SizeBytes = 2048;
    secondBook.File.Sha256Hex = "aabbccddeeff";
    secondBook.AddedAtUtc = std::chrono::system_clock::now();

    try
    {
        static_cast<void>(repository.Add(secondBook));
        FAIL("Expected CDuplicateHashException to be thrown");
    }
    catch (const Librova::Domain::CDuplicateHashException& e)
    {
        REQUIRE(e.ExistingBookId().Value == firstId.Value);
    }

    // Transaction must have been rolled back; only the first book must exist in the catalog.
    {
        Librova::Sqlite::CSqliteConnection conn(databasePath);
        Librova::Sqlite::CSqliteStatement stmt(conn.GetNativeHandle(), "SELECT COUNT(*) FROM books;");
        REQUIRE(stmt.Step());
        REQUIRE(stmt.GetColumnInt(0) == 1);
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book repository ForceAdd inserts book when sha256_hex already exists", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-repository-force-add.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);

    Librova::Domain::SBook firstBook;
    firstBook.Metadata.TitleUtf8 = "First Book";
    firstBook.Metadata.AuthorsUtf8 = {"Author A"};
    firstBook.Metadata.Language = "en";
    firstBook.File.Format = Librova::Domain::EBookFormat::Fb2;
    firstBook.File.StorageEncoding = Librova::Domain::EStorageEncoding::Plain;
    firstBook.File.ManagedPath = std::filesystem::path{"Books/0000000001/first.fb2"};
    firstBook.File.SizeBytes = 1024;
    firstBook.File.Sha256Hex = "aabbccddeeff";
    firstBook.AddedAtUtc = std::chrono::system_clock::now();

    const Librova::Domain::SBookId firstId = repository.Add(firstBook);

    Librova::Domain::SBook secondBook;
    secondBook.Metadata.TitleUtf8 = "Second Book (duplicate allowed)";
    secondBook.Metadata.AuthorsUtf8 = {"Author B"};
    secondBook.Metadata.Language = "en";
    secondBook.File.Format = Librova::Domain::EBookFormat::Fb2;
    secondBook.File.StorageEncoding = Librova::Domain::EStorageEncoding::Plain;
    secondBook.File.ManagedPath = std::filesystem::path{"Books/0000000002/second.fb2"};
    secondBook.File.SizeBytes = 2048;
    secondBook.File.Sha256Hex = "aabbccddeeff";
    secondBook.AddedAtUtc = std::chrono::system_clock::now();

    Librova::Domain::SBookId secondId;
    REQUIRE_NOTHROW(secondId = repository.ForceAdd(secondBook));
    REQUIRE(secondId.IsValid());
    REQUIRE(secondId.Value != firstId.Value);

    REQUIRE(repository.GetById(firstId).has_value());
    REQUIRE(repository.GetById(secondId).has_value());

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book repository Add does not throw for empty sha256_hex even when another book has empty sha256_hex", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-repository-empty-hash.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);

    const auto makeBook = [](const std::string& title, const std::string& path) -> Librova::Domain::SBook {
        Librova::Domain::SBook book;
        book.Metadata.TitleUtf8 = title;
        book.Metadata.AuthorsUtf8 = {"Author"};
        book.Metadata.Language = "en";
        book.File.Format = Librova::Domain::EBookFormat::Fb2;
        book.File.StorageEncoding = Librova::Domain::EStorageEncoding::Plain;
        book.File.ManagedPath = std::filesystem::path{path};
        book.File.SizeBytes = 1024;
        book.File.Sha256Hex = "";
        book.AddedAtUtc = std::chrono::system_clock::now();
        return book;
    };

    REQUIRE_NOTHROW(repository.Add(makeBook("Book One", "Books/0000000001/a.fb2")));
    REQUIRE_NOTHROW(repository.Add(makeBook("Book Two", "Books/0000000002/b.fb2")));

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book repository stores managed_path and cover_path with forward slashes in SQLite on all platforms", "[book-database]")
{
    // Regression guard for C23 (relative paths). PathToUtf8 uses generic_u8string()
    // which always produces forward slashes. This test proves the contract survives
    // round-trips through the database layer even when the in-memory path was
    // constructed with native backslash separators on Windows.
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-repository-path-format.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Path Format Test";
    book.Metadata.AuthorsUtf8 = {"Author"};
    book.Metadata.Language = "en";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.StorageEncoding = Librova::Domain::EStorageEncoding::Plain;
    // Construct with native backslash separators — PathToUtf8 must normalise to forward slashes
    book.File.ManagedPath = std::filesystem::path{L"Books\\0000000200\\book.epub"};
    book.CoverPath = std::filesystem::path{L"Covers\\0000000200.jpg"};
    book.File.SizeBytes = 512;
    book.File.Sha256Hex = "format-test-hash";
    book.AddedAtUtc = std::chrono::system_clock::now();

    const Librova::Domain::SBookId bookId = repository.Add(book);

    {
        Librova::Sqlite::CSqliteConnection conn(databasePath);
        Librova::Sqlite::CSqliteStatement stmt(
            conn.GetNativeHandle(),
            "SELECT managed_path, cover_path FROM books WHERE id = ?;");
        stmt.BindInt64(1, bookId.Value);
        REQUIRE(stmt.Step());

        const std::string storedManagedPath = stmt.GetColumnText(0);
        const std::string storedCoverPath = stmt.GetColumnText(1);

        REQUIRE(storedManagedPath.find('\\') == std::string::npos);
        REQUIRE(storedManagedPath.find('/') != std::string::npos);
        REQUIRE(storedCoverPath.find('\\') == std::string::npos);
        REQUIRE(storedCoverPath.find('/') != std::string::npos);
    }

    // GetById round-trip must also reconstruct paths correctly
    const auto loaded = repository.GetById(bookId);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->File.ManagedPath == book.File.ManagedPath);
    REQUIRE(loaded->CoverPath == book.CoverPath);

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book repository Add is safe under concurrent inserts with the same sha256_hex", "[book-database]")
{
    // Integration guard for C25 (concurrent import race). CSqliteConnection sets
    // busy_timeout=5000ms, so the loser of BEGIN IMMEDIATE waits for the winner
    // to commit. When the loser then executes SELECT sha256_hex inside its own
    // transaction it finds the already-inserted row and throws CDuplicateHashException
    // rather than inserting a second copy.
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-book-repository-concurrent-hash.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    const std::string sharedHash = "concurrent-hash-test-abc123";

    const auto makeBook = [&](const std::string& path) -> Librova::Domain::SBook {
        Librova::Domain::SBook book;
        book.Metadata.TitleUtf8 = "Concurrent Book";
        book.Metadata.AuthorsUtf8 = {"Author"};
        book.Metadata.Language = "en";
        book.File.Format = Librova::Domain::EBookFormat::Fb2;
        book.File.StorageEncoding = Librova::Domain::EStorageEncoding::Plain;
        book.File.ManagedPath = std::filesystem::path{path};
        book.File.SizeBytes = 1024;
        book.File.Sha256Hex = sharedHash;
        book.AddedAtUtc = std::chrono::system_clock::now();
        return book;
    };

    std::atomic<int> successCount{0};
    std::atomic<int> duplicateCount{0};
    std::exception_ptr unexpectedException;
    std::atomic<bool> readyFlag{false};

    const auto runImport = [&](const std::string& path) {
        while (!readyFlag.load(std::memory_order_acquire)) {}
        try
        {
            Librova::BookDatabase::CSqliteBookRepository repo(databasePath);
            static_cast<void>(repo.Add(makeBook(path)));
            successCount.fetch_add(1, std::memory_order_relaxed);
        }
        catch (const Librova::Domain::CDuplicateHashException&)
        {
            duplicateCount.fetch_add(1, std::memory_order_relaxed);
        }
        catch (...)
        {
            unexpectedException = std::current_exception();
        }
    };

    std::thread thread1([&]() { runImport("Books/0000000001/concurrent.fb2"); });
    std::thread thread2([&]() { runImport("Books/0000000002/concurrent.fb2"); });

    readyFlag.store(true, std::memory_order_release);
    thread1.join();
    thread2.join();

    REQUIRE(unexpectedException == nullptr);
    REQUIRE(successCount.load() + duplicateCount.load() == 2);
    REQUIRE(successCount.load() == 1);
    REQUIRE(duplicateCount.load() == 1);

    {
        Librova::Sqlite::CSqliteConnection conn(databasePath);
        Librova::Sqlite::CSqliteStatement stmt(conn.GetNativeHandle(), "SELECT COUNT(*) FROM books;");
        REQUIRE(stmt.Step());
        REQUIRE(stmt.GetColumnInt(0) == 1);
    }

    std::filesystem::remove(databasePath);
}
