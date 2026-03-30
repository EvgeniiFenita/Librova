#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>

#include "Application/LibraryCatalogFacade.hpp"
#include "BookDatabase/SqliteBookQueryRepository.hpp"
#include "BookDatabase/SqliteBookRepository.hpp"
#include "DatabaseRuntime/SchemaMigrator.hpp"

namespace {

Librova::Domain::SBook MakeBook(
    const std::string_view title,
    const std::vector<std::string>& authors,
    const std::string_view language,
    const Librova::Domain::EBookFormat format,
    const std::filesystem::path& managedPath,
    const std::string_view hash,
    const std::chrono::system_clock::time_point addedAtUtc)
{
    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = std::string{title};
    book.Metadata.AuthorsUtf8 = authors;
    book.Metadata.Language = std::string{language};
    book.File.Format = format;
    book.File.ManagedPath = managedPath;
    book.File.SizeBytes = 4096;
    book.File.Sha256Hex = std::string{hash};
    book.AddedAtUtc = addedAtUtc;
    return book;
}

} // namespace

TEST_CASE("Library catalog facade returns mapped list items from sqlite read side", "[application][catalog]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-library-catalog.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook firstBook = MakeBook(
        "Пикник на обочине",
        {"Аркадий Стругацкий", "Борис Стругацкий"},
        "ru",
        Librova::Domain::EBookFormat::Epub,
        "Books/0000001001/book.epub",
        "catalog-hash-1",
        std::chrono::sys_days{std::chrono::March / 30 / 2026});
    firstBook.Metadata.TagsUtf8 = {"classic", "sci-fi"};
    firstBook.Metadata.SeriesUtf8 = std::string{"Миры"};
    firstBook.Metadata.SeriesIndex = 1.0;
    firstBook.Metadata.Year = 1972;
    firstBook.CoverPath = std::filesystem::path("Covers/0000001001.jpg");
    static_cast<void>(writeRepository.Add(firstBook));

    Librova::Domain::SBook secondBook = MakeBook(
        "Roadside Picnic",
        {"Arkady Strugatsky"},
        "en",
        Librova::Domain::EBookFormat::Fb2,
        "Books/0000001002/book.fb2",
        "catalog-hash-2",
        std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{1});
    secondBook.Metadata.TagsUtf8 = {"translated", "zone"};
    secondBook.Metadata.DescriptionUtf8 = std::string{"A dangerous zone full of artifacts"};
    static_cast<void>(writeRepository.Add(secondBook));

    const Librova::Application::CLibraryCatalogFacade facade(queryRepository);
    const Librova::Application::SBookListResult result = facade.ListBooks({
        .TextUtf8 = "zone",
        .SortBy = Librova::Domain::EBookSort::DateAdded,
        .Limit = 10
    });

    REQUIRE_FALSE(result.IsEmpty());
    REQUIRE(result.Items.size() == 1);
    REQUIRE(result.Items.front().TitleUtf8 == "Roadside Picnic");
    REQUIRE(result.Items.front().AuthorsUtf8 == std::vector<std::string>({"Arkady Strugatsky"}));
    REQUIRE(result.Items.front().Language == "en");
    REQUIRE(result.Items.front().Format == Librova::Domain::EBookFormat::Fb2);
    REQUIRE(result.Items.front().ManagedPath == std::filesystem::path("Books/0000001002/book.fb2"));
    REQUIRE_FALSE(result.Items.front().CoverPath.has_value());

    std::filesystem::remove(databasePath);
}

TEST_CASE("Library catalog facade respects pagination and structured filters", "[application][catalog]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-library-catalog-pagination.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook firstBook = MakeBook(
        "Alpha",
        {"Author One"},
        "en",
        Librova::Domain::EBookFormat::Epub,
        "Books/0000001101/alpha.epub",
        "catalog-page-1",
        std::chrono::sys_days{std::chrono::March / 30 / 2026});
    firstBook.Metadata.TagsUtf8 = {"selected"};
    static_cast<void>(writeRepository.Add(firstBook));

    Librova::Domain::SBook secondBook = MakeBook(
        "Beta",
        {"Author Two"},
        "en",
        Librova::Domain::EBookFormat::Epub,
        "Books/0000001102/beta.epub",
        "catalog-page-2",
        std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{1});
    secondBook.Metadata.TagsUtf8 = {"selected"};
    static_cast<void>(writeRepository.Add(secondBook));

    const Librova::Application::CLibraryCatalogFacade facade(queryRepository);
    const Librova::Application::SBookListResult result = facade.ListBooks({
        .Language = std::string{"en"},
        .TagsUtf8 = {"selected"},
        .Format = Librova::Domain::EBookFormat::Epub,
        .SortBy = Librova::Domain::EBookSort::Title,
        .Offset = 1,
        .Limit = 1
    });

    REQUIRE(result.Items.size() == 1);
    REQUIRE(result.Items.front().TitleUtf8 == "Beta");

    std::filesystem::remove(databasePath);
}

TEST_CASE("Library catalog facade rejects zero page size", "[application][catalog]")
{
    class CUnusedQueryRepository final : public Librova::Domain::IBookQueryRepository
    {
    public:
        [[nodiscard]] std::vector<Librova::Domain::SBook> Search(const Librova::Domain::SSearchQuery&) const override
        {
            return {};
        }

        [[nodiscard]] std::vector<Librova::Domain::SDuplicateMatch> FindDuplicates(const Librova::Domain::SCandidateBook&) const override
        {
            return {};
        }
    };

    const CUnusedQueryRepository repository;
    const Librova::Application::CLibraryCatalogFacade facade(repository);

    REQUIRE_THROWS_AS(
        facade.ListBooks({
            .TextUtf8 = "anything",
            .Limit = 0
        }),
        std::invalid_argument);
}
