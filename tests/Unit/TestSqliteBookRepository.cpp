#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>

#include "BookDatabase/SqliteBookRepository.hpp"
#include "DatabaseRuntime/SchemaMigrator.hpp"
#include "Sqlite/SqliteConnection.hpp"
#include "Sqlite/SqliteStatement.hpp"

TEST_CASE("Sqlite book repository round-trips book metadata and files", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "libriflow-book-repository.db";
    std::filesystem::remove(databasePath);
    LibriFlow::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    LibriFlow::BookDatabase::CSqliteBookRepository repository(databasePath);

    LibriFlow::Domain::SBook book;
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
    book.File.Format = LibriFlow::Domain::EBookFormat::Epub;
    book.File.ManagedPath = std::filesystem::path(u8"Books/0000000001/Пикник.epub");
    book.File.SizeBytes = 4096;
    book.File.Sha256Hex = "abc123";
    book.CoverPath = std::filesystem::path(u8"Covers/0000000001.jpg");
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026} + std::chrono::hours{14} + std::chrono::minutes{5};

    const LibriFlow::Domain::SBookId bookId = repository.Add(book);
    REQUIRE(bookId.IsValid());

    const std::optional<LibriFlow::Domain::SBook> loadedBook = repository.GetById(bookId);

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
    REQUIRE(loadedBook->File.ManagedPath == book.File.ManagedPath);
    REQUIRE(loadedBook->File.SizeBytes == book.File.SizeBytes);
    REQUIRE(loadedBook->File.Sha256Hex == book.File.Sha256Hex);
    REQUIRE(loadedBook->CoverPath == book.CoverPath);
    REQUIRE(loadedBook->AddedAtUtc == book.AddedAtUtc);

    {
        LibriFlow::Sqlite::CSqliteConnection connection(databasePath);
        LibriFlow::Sqlite::CSqliteStatement authorStatement(
            connection.GetNativeHandle(),
            "SELECT COUNT(*) FROM authors;");
        REQUIRE(authorStatement.Step());
        REQUIRE(authorStatement.GetColumnInt(0) == 2);

        LibriFlow::Sqlite::CSqliteStatement tagStatement(
            connection.GetNativeHandle(),
            "SELECT COUNT(*) FROM tags;");
        REQUIRE(tagStatement.Step());
        REQUIRE(tagStatement.GetColumnInt(0) == 2);
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite book repository removes stored books", "[book-database]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "libriflow-book-repository-remove.db";
    std::filesystem::remove(databasePath);
    LibriFlow::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    LibriFlow::BookDatabase::CSqliteBookRepository repository(databasePath);

    LibriFlow::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Roadside Picnic";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.File.ManagedPath = "Books/0000000002/book.epub";
    book.File.SizeBytes = 1024;
    book.File.Sha256Hex = "deadbeef";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};

    const LibriFlow::Domain::SBookId bookId = repository.Add(book);
    REQUIRE(repository.GetById(bookId).has_value());

    repository.Remove(bookId);

    REQUIRE_FALSE(repository.GetById(bookId).has_value());
    std::filesystem::remove(databasePath);
}
