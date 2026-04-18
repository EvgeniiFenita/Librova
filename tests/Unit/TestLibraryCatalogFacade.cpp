#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

#include "Application/LibraryCatalogFacade.hpp"
#include "Database/SqliteBookQueryRepository.hpp"
#include "Database/SqliteBookRepository.hpp"
#include "Database/SchemaMigrator.hpp"

namespace {

class CEmptyBookRepository final : public Librova::Domain::IBookRepository
{
public:
    [[nodiscard]] Librova::Domain::SBookId ReserveId() override
    {
        return {1};
    }

    [[nodiscard]] Librova::Domain::SBookId Add(const Librova::Domain::SBook& book) override
    {
        return book.Id;
    }

    [[nodiscard]] Librova::Domain::SBookId ForceAdd(const Librova::Domain::SBook& book) override
    {
        return book.Id;
    }

    [[nodiscard]] std::optional<Librova::Domain::SBook> GetById(const Librova::Domain::SBookId) const override
    {
        return std::nullopt;
    }

    void Remove(const Librova::Domain::SBookId) override
    {
    }
};

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

    {
        Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
        Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

        Librova::Domain::SBook firstBook = MakeBook(
            "Пикник на обочине",
            {"Аркадий Стругацкий", "Борис Стругацкий"},
            "ru",
            Librova::Domain::EBookFormat::Epub,
            "Objects/ef/e2/0000001001.book.epub",
            "catalog-hash-1",
            std::chrono::sys_days{std::chrono::March / 30 / 2026});
        firstBook.Metadata.TagsUtf8 = {"classic", "sci-fi"};
        firstBook.Metadata.SeriesUtf8 = std::string{"Миры"};
        firstBook.Metadata.SeriesIndex = 1.0;
        firstBook.Metadata.Year = 1972;
        firstBook.CoverPath = std::filesystem::path("Objects/ef/e2/0000001001.cover.jpg");
        static_cast<void>(writeRepository.Add(firstBook));

        Librova::Domain::SBook secondBook = MakeBook(
            "Roadside Picnic",
            {"Arkady Strugatsky"},
            "en",
            Librova::Domain::EBookFormat::Fb2,
            "Objects/82/e4/0000001002.book.fb2",
            "catalog-hash-2",
            std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{1});
        secondBook.Metadata.GenresUtf8 = {"translated", "zone"};
        secondBook.Metadata.DescriptionUtf8 = std::string{"A dangerous zone full of artifacts"};
        static_cast<void>(writeRepository.Add(secondBook));

        const Librova::Application::CLibraryCatalogFacade facade(queryRepository, writeRepository);
        const Librova::Application::SBookListResult result = facade.ListBooks({
            .TextUtf8 = "zone",
            .SortBy = Librova::Domain::EBookSort::DateAdded,
            .Limit = 10
        });

        REQUIRE_FALSE(result.IsEmpty());
        REQUIRE(result.Items.size() == 1);
        REQUIRE(result.TotalCount == 1);
        REQUIRE(result.AvailableLanguages == std::vector<std::string>({"en"}));
        REQUIRE(result.AvailableGenres == std::vector<std::string>({"translated", "zone"}));
        REQUIRE(result.Statistics.BookCount == 2);
        REQUIRE(result.Statistics.TotalManagedBookSizeBytes == 8192);
        REQUIRE(result.Statistics.TotalLibrarySizeBytes > 8192);
        REQUIRE(result.Items.front().TitleUtf8 == "Roadside Picnic");
        REQUIRE(result.Items.front().AuthorsUtf8 == std::vector<std::string>({"Arkady Strugatsky"}));
        REQUIRE(result.Items.front().Language == "en");
        REQUIRE(result.Items.front().Format == Librova::Domain::EBookFormat::Fb2);
        REQUIRE(result.Items.front().ManagedPath == std::filesystem::path("Objects/82/e4/0000001002.book.fb2"));
        REQUIRE_FALSE(result.Items.front().CoverPath.has_value());
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Library catalog facade respects pagination and structured filters", "[application][catalog]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-library-catalog-pagination.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    {
        Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
        Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook firstBook = MakeBook(
        "Alpha",
        {"Author One"},
        "en",
        Librova::Domain::EBookFormat::Epub,
        "Objects/f4/45/0000001101.book.alpha.epub",
        "catalog-page-1",
        std::chrono::sys_days{std::chrono::March / 30 / 2026});
    firstBook.Metadata.GenresUtf8 = {"selected"};
    static_cast<void>(writeRepository.Add(firstBook));

    Librova::Domain::SBook secondBook = MakeBook(
        "Beta",
        {"Author Two"},
        "en",
        Librova::Domain::EBookFormat::Epub,
        "Objects/ad/4a/0000001102.book.beta.epub",
        "catalog-page-2",
        std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{1});
    secondBook.Metadata.GenresUtf8 = {"selected"};
    static_cast<void>(writeRepository.Add(secondBook));

    Librova::Domain::SBook thirdBook = MakeBook(
        "Gamma",
        {"Author Three"},
        "ru",
        Librova::Domain::EBookFormat::Epub,
        "Objects/1a/49/0000001103.book.gamma.epub",
        "catalog-page-3",
        std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{2});
    thirdBook.Metadata.GenresUtf8 = {"selected"};
    static_cast<void>(writeRepository.Add(thirdBook));

        const Librova::Application::CLibraryCatalogFacade facade(queryRepository, writeRepository);
        const Librova::Application::SBookListResult result = facade.ListBooks({
            .Languages = {"en"},
            .GenresUtf8 = {"selected"},
            .Format = Librova::Domain::EBookFormat::Epub,
            .SortBy = Librova::Domain::EBookSort::Title,
            .Offset = 1,
            .Limit = 1
        });

        REQUIRE(result.Items.size() == 1);
        REQUIRE(result.TotalCount == 2);
        REQUIRE(result.AvailableLanguages == std::vector<std::string>({"en", "ru"}));
        REQUIRE(result.AvailableGenres == std::vector<std::string>({"selected"}));
        REQUIRE(result.Statistics.BookCount == 3);
        REQUIRE(result.Items.front().TitleUtf8 == "Beta");
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Library catalog facade sorts by title descending when direction is Descending", "[application][catalog]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-catalog-sort-direction.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    {
        Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
        Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    static_cast<void>(writeRepository.Add(MakeBook(
        "Alpha", {"Author A"}, "en",
        Librova::Domain::EBookFormat::Epub, "Objects/9c/c6/0000002001.book.alpha.epub",
        "dir-hash-1", std::chrono::sys_days{std::chrono::March / 30 / 2026})));
    static_cast<void>(writeRepository.Add(MakeBook(
        "Beta", {"Author B"}, "en",
        Librova::Domain::EBookFormat::Epub, "Objects/55/cb/0000002002.book.beta.epub",
        "dir-hash-2", std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{1})));
    static_cast<void>(writeRepository.Add(MakeBook(
        "Gamma", {"Author C"}, "en",
        Librova::Domain::EBookFormat::Epub, "Objects/c2/c9/0000002003.book.gamma.epub",
        "dir-hash-3", std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{2})));

        const Librova::Application::CLibraryCatalogFacade facade(queryRepository, writeRepository);

        const auto ascending = facade.ListBooks({
            .SortBy = Librova::Domain::EBookSort::Title,
            .SortDirection = Librova::Domain::ESortDirection::Ascending,
            .Limit = 10
        });
        REQUIRE(ascending.Items.size() == 3);
        REQUIRE(ascending.Items[0].TitleUtf8 == "Alpha");
        REQUIRE(ascending.Items[1].TitleUtf8 == "Beta");
        REQUIRE(ascending.Items[2].TitleUtf8 == "Gamma");

        const auto descending = facade.ListBooks({
            .SortBy = Librova::Domain::EBookSort::Title,
            .SortDirection = Librova::Domain::ESortDirection::Descending,
            .Limit = 10
        });
        REQUIRE(descending.Items.size() == 3);
        REQUIRE(descending.Items[0].TitleUtf8 == "Gamma");
        REQUIRE(descending.Items[1].TitleUtf8 == "Beta");
        REQUIRE(descending.Items[2].TitleUtf8 == "Alpha");
    }

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

        [[nodiscard]] std::uint64_t CountSearchResults(const Librova::Domain::SSearchQuery&) const override
        {
            return 0;
        }

        [[nodiscard]] std::vector<std::string> ListAvailableLanguages(const Librova::Domain::SSearchQuery&) const override
        {
            return {};
        }

        [[nodiscard]] std::vector<std::string> ListAvailableTags(const Librova::Domain::SSearchQuery&) const override
        {
            return {};
        }

        [[nodiscard]] std::vector<std::string> ListAvailableGenres(const Librova::Domain::SSearchQuery&) const override
        {
            return {};
        }

        [[nodiscard]] std::vector<Librova::Domain::SDuplicateMatch> FindDuplicates(const Librova::Domain::SCandidateBook&) const override
        {
            return {};
        }

        [[nodiscard]] Librova::Domain::IBookQueryRepository::SLibraryStatistics GetLibraryStatistics() const override
        {
            return {};
        }
    };

    const CUnusedQueryRepository repository;
    CEmptyBookRepository bookRepository;
    const Librova::Application::CLibraryCatalogFacade facade(repository, bookRepository);

    REQUIRE_THROWS_AS(
        facade.ListBooks({
            .TextUtf8 = "anything",
            .Limit = 0
        }),
        std::invalid_argument);
}

TEST_CASE("Library catalog facade returns full book details by id", "[application][catalog]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-library-catalog-details.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    {
        Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
        Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book = MakeBook(
        "Definitely Maybe",
        {"Arkady Strugatsky", "Boris Strugatsky"},
        "en",
        Librova::Domain::EBookFormat::Fb2,
        "Objects/85/ff/0000001201.book.fb2",
        "details-hash-1",
        std::chrono::sys_days{std::chrono::March / 30 / 2026});
    book.Metadata.PublisherUtf8 = std::string{"Macmillan"};
    book.Metadata.Year = 1978;
    book.Metadata.Isbn = std::string{"978-5-17-000000-1"};
    book.Metadata.DescriptionUtf8 = std::string{"Aliens land only in one city."};
    book.Metadata.Identifier = std::string{"details-id-1"};
    book.Metadata.TagsUtf8 = {"classic", "first-contact"};
    book.CoverPath = std::filesystem::path("Objects/85/ff/0000001201.cover.jpg");
    const auto addedId = writeRepository.Add(book);

        const Librova::Application::CLibraryCatalogFacade facade(queryRepository, writeRepository);
        const auto details = facade.GetBookDetails(addedId);

        REQUIRE(details.has_value());
        REQUIRE(details->Id.Value == addedId.Value);
        REQUIRE(details->TitleUtf8 == "Definitely Maybe");
        REQUIRE(details->PublisherUtf8 == std::optional<std::string>{"Macmillan"});
        REQUIRE(details->Isbn == std::optional<std::string>{"9785170000001"});
        REQUIRE(details->DescriptionUtf8 == std::optional<std::string>{"Aliens land only in one city."});
        REQUIRE(details->Identifier == std::optional<std::string>{"details-id-1"});
        REQUIRE(details->Sha256Hex == "details-hash-1");
        REQUIRE(details->CoverPath == std::optional<std::filesystem::path>{std::filesystem::path("Objects/85/ff/0000001201.cover.jpg")});
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Library catalog facade returns aggregate library statistics", "[application][catalog]")
{
    const std::filesystem::path libraryRoot = std::filesystem::temp_directory_path() / "librova-library-catalog-statistics";
    const std::filesystem::path databasePath = libraryRoot / "Database" / "librova.db";
    std::filesystem::remove_all(libraryRoot);
    std::filesystem::create_directories(databasePath.parent_path());
    std::filesystem::create_directories(libraryRoot / "Objects/8a/62");
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    {
        Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
        Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook firstBook = MakeBook(
        "Alpha",
        {"Author One"},
        "en",
        Librova::Domain::EBookFormat::Epub,
        "Objects/8a/62/0000001301.book.epub",
        "stats-hash-1",
        std::chrono::sys_days{std::chrono::March / 30 / 2026});
    firstBook.File.SizeBytes = 1024;
    firstBook.CoverPath = std::filesystem::path("Objects/8a/62/0000001301.cover.png");
    static_cast<void>(writeRepository.Add(firstBook));

    Librova::Domain::SBook secondBook = MakeBook(
        "Beta",
        {"Author Two"},
        "en",
        Librova::Domain::EBookFormat::Fb2,
        "Objects/f7/60/0000001302.book.fb2",
        "stats-hash-2",
        std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{1});
    secondBook.File.SizeBytes = 2048;
    static_cast<void>(writeRepository.Add(secondBook));

        std::ofstream(libraryRoot / "Objects/8a/62/0000001301.cover.png", std::ios::binary) << "cover-bytes";

        const Librova::Application::CLibraryCatalogFacade facade(queryRepository, writeRepository);
        const auto statistics = facade.GetLibraryStatistics();
        const auto coverSize = static_cast<std::uint64_t>(std::filesystem::file_size(libraryRoot / "Objects/8a/62/0000001301.cover.png"));
        const auto databaseSize = static_cast<std::uint64_t>(std::filesystem::file_size(databasePath));

        REQUIRE(statistics.BookCount == 2);
        REQUIRE(statistics.TotalManagedBookSizeBytes == 3072);
        REQUIRE(statistics.TotalLibrarySizeBytes == 3072 + coverSize + databaseSize);
    }

    std::filesystem::remove_all(libraryRoot);
}

TEST_CASE("Library catalog facade filters books by multiple languages", "[application][catalog]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-catalog-multi-language.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    {
        Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
        Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

        static_cast<void>(writeRepository.Add(MakeBook("English Book", {"Author A"}, "en",
            Librova::Domain::EBookFormat::Epub, "Objects/77/cc/0000002101.book.en.epub", "ml-hash-1",
            std::chrono::sys_days{std::chrono::March / 30 / 2026})));
        static_cast<void>(writeRepository.Add(MakeBook("Russian Book", {"Author B"}, "ru",
            Librova::Domain::EBookFormat::Epub, "Objects/0a/ce/0000002102.book.ru.epub", "ml-hash-2",
            std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{1})));
        static_cast<void>(writeRepository.Add(MakeBook("German Book", {"Author C"}, "de",
            Librova::Domain::EBookFormat::Epub, "Objects/9d/cf/0000002103.book.de.epub", "ml-hash-3",
            std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{2})));

        const Librova::Application::CLibraryCatalogFacade facade(queryRepository, writeRepository);
        const Librova::Application::SBookListResult result = facade.ListBooks({
            .Languages = {"en", "ru"},
            .SortBy = Librova::Domain::EBookSort::Title,
            .Limit = 10
        });

        REQUIRE(result.Items.size() == 2);
        REQUIRE(result.TotalCount == 2);
        REQUIRE(result.Items[0].TitleUtf8 == "English Book");
        REQUIRE(result.Items[1].TitleUtf8 == "Russian Book");
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Library catalog facade filters books by multiple genres with OR semantics", "[application][catalog]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-catalog-multi-genre.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    {
        Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
        Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

        Librova::Domain::SBook scifiBook = MakeBook("Sci-Fi Book", {"Author X"}, "en",
            Librova::Domain::EBookFormat::Epub, "Objects/12/95/0000002201.book.scifi.epub", "mg-hash-1",
            std::chrono::sys_days{std::chrono::March / 30 / 2026});
        scifiBook.Metadata.GenresUtf8 = {"sci-fi"};
        static_cast<void>(writeRepository.Add(scifiBook));

        Librova::Domain::SBook classicBook = MakeBook("Classic Book", {"Author Y"}, "en",
            Librova::Domain::EBookFormat::Epub, "Objects/7f/93/0000002202.book.classic.epub", "mg-hash-2",
            std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{1});
        classicBook.Metadata.GenresUtf8 = {"classic"};
        static_cast<void>(writeRepository.Add(classicBook));

        Librova::Domain::SBook otherBook = MakeBook("Other Book", {"Author Z"}, "en",
            Librova::Domain::EBookFormat::Epub, "Objects/ec/91/0000002203.book.other.epub", "mg-hash-3",
            std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{2});
        otherBook.Metadata.GenresUtf8 = {"thriller"};
        static_cast<void>(writeRepository.Add(otherBook));

        const Librova::Application::CLibraryCatalogFacade facade(queryRepository, writeRepository);
        const Librova::Application::SBookListResult result = facade.ListBooks({
            .GenresUtf8 = {"sci-fi", "classic"},
            .SortBy = Librova::Domain::EBookSort::Title,
            .Limit = 10
        });

        REQUIRE(result.Items.size() == 2);
        REQUIRE(result.TotalCount == 2);
        REQUIRE(result.Items[0].TitleUtf8 == "Classic Book");
        REQUIRE(result.Items[1].TitleUtf8 == "Sci-Fi Book");
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Library catalog facade applies independent genre and tag filters", "[application][catalog]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-catalog-genre-tag-combined.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    {
        Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
        Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

        // Book A: genre "sci-fi", tag "award-winner" -> matches genre+tag filter
        Librova::Domain::SBook bookA = MakeBook("Book A", {"Author A"}, "en",
            Librova::Domain::EBookFormat::Epub, "Objects/6b/4c/0000002501.book.a.epub", "gt-hash-1",
            std::chrono::sys_days{std::chrono::March / 30 / 2026});
        bookA.Metadata.GenresUtf8 = {"sci-fi"};
        bookA.Metadata.TagsUtf8 = {"award-winner"};
        static_cast<void>(writeRepository.Add(bookA));

        // Book B: genre "sci-fi", wrong tag -> should NOT match
        Librova::Domain::SBook bookB = MakeBook("Book B", {"Author B"}, "en",
            Librova::Domain::EBookFormat::Epub, "Objects/fe/4d/0000002502.book.b.epub", "gt-hash-2",
            std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{1});
        bookB.Metadata.GenresUtf8 = {"sci-fi"};
        bookB.Metadata.TagsUtf8 = {"bestseller"};
        static_cast<void>(writeRepository.Add(bookB));

        // Book C: matching tag, wrong genre -> should NOT match
        Librova::Domain::SBook bookC = MakeBook("Book C", {"Author C"}, "en",
            Librova::Domain::EBookFormat::Epub, "Objects/91/4f/0000002503.book.c.epub", "gt-hash-3",
            std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{2});
        bookC.Metadata.GenresUtf8 = {"thriller"};
        bookC.Metadata.TagsUtf8 = {"award-winner"};
        static_cast<void>(writeRepository.Add(bookC));

        const Librova::Application::CLibraryCatalogFacade facade(queryRepository, writeRepository);
        const Librova::Application::SBookListResult result = facade.ListBooks({
            .GenresUtf8 = {"sci-fi"},
            .TagsUtf8 = {"award-winner"},
            .SortBy = Librova::Domain::EBookSort::Title,
            .Limit = 10
        });

        REQUIRE(result.Items.size() == 1);
        REQUIRE(result.TotalCount == 1);
        REQUIRE(result.Items.front().TitleUtf8 == "Book A");
    }

    std::filesystem::remove(databasePath);
}


