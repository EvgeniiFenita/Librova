#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Application/ImportRollbackService.hpp"
#include "BookDatabase/SqliteBookRepository.hpp"
#include "DatabaseRuntime/SchemaMigrator.hpp"
#include "Domain/Book.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace {

struct SRollbackSandbox
{
    std::filesystem::path Root;
    std::filesystem::path DatabasePath;

    ~SRollbackSandbox()
    {
        std::error_code errorCode;
        std::filesystem::remove_all(Root, errorCode);
    }
};

SRollbackSandbox CreateRollbackSandbox(std::string_view scenario)
{
    const auto root = std::filesystem::temp_directory_path()
        / ("librova-rollback-" + std::string{scenario});
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "Books");
    std::filesystem::create_directories(root / "Covers");

    const auto dbPath = root / "librova.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(dbPath);

    return {.Root = root, .DatabasePath = dbPath};
}

void WriteFile(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path).put('x');
}

Librova::Domain::SBook BuildBook(
    const std::filesystem::path& managedFilePath,
    std::optional<std::filesystem::path> coverPath = std::nullopt)
{
    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Test Book";
    book.Metadata.AuthorsUtf8 = {"Author"};
    book.Metadata.Language = "en";
    book.File.Format = Librova::Domain::EBookFormat::Fb2;
    book.File.StorageEncoding = Librova::Domain::EStorageEncoding::Plain;
    book.File.ManagedPath = managedFilePath;
    book.File.SizeBytes = 1;
    book.File.Sha256Hex = "aa";
    book.CoverPath = coverPath;
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::January / 1 / 2026};
    return book;
}

} // namespace

TEST_CASE("Rollback does not remove top-level Books directory when it becomes empty", "[rollback]")
{
    auto sandbox = CreateRollbackSandbox("preserve-books-dir");
    Librova::BookDatabase::CSqliteBookRepository repository(sandbox.DatabasePath);

    const auto managedFilePath = std::filesystem::path(u8"Books") / u8"book.fb2";
    WriteFile(sandbox.Root / managedFilePath);

    const auto bookId = repository.Add(BuildBook(managedFilePath));
    REQUIRE(bookId.IsValid());

    Librova::Application::CImportRollbackService rollback(repository, sandbox.Root);
    const auto result = rollback.RollbackImportedBooks({bookId});

    REQUIRE(result.RemainingBookIds.empty());
    REQUIRE_FALSE(std::filesystem::exists(sandbox.Root / managedFilePath));
    REQUIRE(std::filesystem::is_directory(sandbox.Root / "Books"));

    repository.CloseSession();
}

TEST_CASE("Rollback removes the empty managed directory of a cancelled book", "[rollback]")
{
    auto sandbox = CreateRollbackSandbox("remove-empty-book-dir");
    Librova::BookDatabase::CSqliteBookRepository repository(sandbox.DatabasePath);

    const auto managedFilePath = std::filesystem::path(u8"Books") / u8"0000000001" / u8"book.fb2";
    WriteFile(sandbox.Root / managedFilePath);

    const auto bookId = repository.Add(BuildBook(managedFilePath));
    REQUIRE(bookId.IsValid());

    Librova::Application::CImportRollbackService rollback(repository, sandbox.Root);
    const auto result = rollback.RollbackImportedBooks({bookId});

    REQUIRE(result.RemainingBookIds.empty());
    REQUIRE_FALSE(std::filesystem::exists(sandbox.Root / managedFilePath));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.Root / "Books" / "0000000001"));
    REQUIRE(std::filesystem::is_directory(sandbox.Root / "Books"));

    repository.CloseSession();
}

TEST_CASE("Rollback removes only cancelled book files and leaves parent directories intact", "[rollback]")
{
    auto sandbox = CreateRollbackSandbox("preserve-parent-subdir");
    Librova::BookDatabase::CSqliteBookRepository repository(sandbox.DatabasePath);

    const auto subDir = std::filesystem::path(u8"Books") / u8"SomeAuthor";
    const auto managedFilePath = subDir / u8"book.fb2";
    WriteFile(sandbox.Root / managedFilePath);

    const auto bookId = repository.Add(BuildBook(managedFilePath));
    REQUIRE(bookId.IsValid());

    Librova::Application::CImportRollbackService rollback(repository, sandbox.Root);
    const auto result = rollback.RollbackImportedBooks({bookId});

    REQUIRE(result.RemainingBookIds.empty());
    REQUIRE_FALSE(std::filesystem::exists(sandbox.Root / managedFilePath));
    REQUIRE(std::filesystem::is_directory(sandbox.Root / subDir));
    REQUIRE(std::filesystem::is_directory(sandbox.Root / "Books"));

    repository.CloseSession();
}

TEST_CASE("Rollback removes empty cover subdir but preserves Covers top-level directory", "[rollback]")
{
    auto sandbox = CreateRollbackSandbox("preserve-covers-dir");
    Librova::BookDatabase::CSqliteBookRepository repository(sandbox.DatabasePath);

    const auto managedFilePath = std::filesystem::path(u8"Books") / u8"book.fb2";
    const auto coverPath = std::filesystem::path(u8"Covers") / u8"0000000001.jpg";
    WriteFile(sandbox.Root / managedFilePath);
    WriteFile(sandbox.Root / coverPath);

    const auto bookId = repository.Add(BuildBook(managedFilePath, coverPath));
    REQUIRE(bookId.IsValid());

    Librova::Application::CImportRollbackService rollback(repository, sandbox.Root);
    const auto result = rollback.RollbackImportedBooks({bookId});

    REQUIRE(result.RemainingBookIds.empty());
    REQUIRE_FALSE(std::filesystem::exists(sandbox.Root / coverPath));
    REQUIRE(std::filesystem::is_directory(sandbox.Root / "Covers"));

    repository.CloseSession();
}

TEST_CASE("Rollback service invokes progress callback after each batch and at completion", "[rollback]")
{
    auto sandbox = CreateRollbackSandbox("progress-callback");
    Librova::BookDatabase::CSqliteBookRepository repository(sandbox.DatabasePath);

    std::vector<Librova::Domain::SBookId> bookIds;
    for (int i = 0; i < 3; ++i)
    {
        const auto managedFilePath =
            std::filesystem::path(u8"Books") / ("book-" + std::to_string(i) + ".fb2");
        WriteFile(sandbox.Root / managedFilePath);

        Librova::Domain::SBook book = BuildBook(managedFilePath);
        book.File.Sha256Hex = "sha-" + std::to_string(i);
        const auto id = repository.Add(book);
        REQUIRE(id.IsValid());
        bookIds.push_back(id);
    }

    std::size_t lastRolledBack = 0;
    std::size_t lastTotal = 0;
    int callbackCount = 0;

    Librova::Application::CImportRollbackService rollback(repository, sandbox.Root);
    const auto result = rollback.RollbackImportedBooks(
        bookIds,
        [&](std::size_t rolledBack, std::size_t total)
        {
            lastRolledBack = rolledBack;
            lastTotal = total;
            ++callbackCount;
        });

    REQUIRE(result.RemainingBookIds.empty());
    // Callback must be invoked at least once (completion call).
    REQUIRE(callbackCount > 0);
    // Final call must report all books rolled back.
    REQUIRE(lastRolledBack == 3);
    REQUIRE(lastTotal == 3);

    repository.CloseSession();
}
