#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "Application/ImportRollbackService.hpp"
#include "Database/SqliteBookRepository.hpp"
#include "Database/SchemaMigrator.hpp"
#include "Domain/Book.hpp"
#include "Foundation/Logging.hpp"
#include "Storage/ManagedLibraryLayout.hpp"
#include "Foundation/UnicodeConversion.hpp"

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
    std::filesystem::create_directories(root / "Objects");
    std::filesystem::create_directories(root / "Logs");

    const auto dbPath = root / "librova.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(dbPath);

    return {.Root = root, .DatabasePath = dbPath};
}

void WriteFile(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path).put('x');
}

std::filesystem::path BuildRelativeShardDirectory(const Librova::Domain::SBookId bookId)
{
    const auto libraryRoot = std::filesystem::path{"LibraryRoot"};
    return Librova::StoragePlanning::CManagedLibraryLayout::GetObjectShardDirectory(libraryRoot, bookId)
        .lexically_relative(libraryRoot);
}

std::filesystem::path BuildRelativeManagedBookPath(const Librova::Domain::SBookId bookId)
{
    const auto libraryRoot = std::filesystem::path{"LibraryRoot"};
    return Librova::StoragePlanning::CManagedLibraryLayout::GetManagedBookPath(
               libraryRoot,
               bookId,
               Librova::Domain::EBookFormat::Fb2)
        .lexically_relative(libraryRoot);
}

std::filesystem::path BuildRelativeCoverPath(
    const Librova::Domain::SBookId bookId,
    std::string_view extension)
{
    const auto libraryRoot = std::filesystem::path{"LibraryRoot"};
    return Librova::StoragePlanning::CManagedLibraryLayout::GetCoverPath(libraryRoot, bookId, extension)
        .lexically_relative(libraryRoot);
}

std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
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

TEST_CASE("Rollback does not remove top-level Objects directory when it becomes empty", "[rollback]")
{
    auto sandbox = CreateRollbackSandbox("preserve-objects-dir");
    Librova::BookDatabase::CSqliteBookRepository repository(sandbox.DatabasePath);

    const auto managedFilePath = BuildRelativeManagedBookPath({1});
    WriteFile(sandbox.Root / managedFilePath);

    const auto bookId = repository.Add(BuildBook(managedFilePath));
    REQUIRE(bookId.IsValid());

    Librova::Application::CImportRollbackService rollback(repository, sandbox.Root);
    const auto result = rollback.RollbackImportedBooks({bookId});

    REQUIRE(result.RemainingBookIds.empty());
    REQUIRE_FALSE(std::filesystem::exists(sandbox.Root / managedFilePath));
    REQUIRE(std::filesystem::is_directory(sandbox.Root / "Objects"));

    repository.CloseSession();
}

TEST_CASE("Rollback removes the empty shard directory of a cancelled book", "[rollback]")
{
    auto sandbox = CreateRollbackSandbox("remove-empty-shard-dir");
    Librova::BookDatabase::CSqliteBookRepository repository(sandbox.DatabasePath);

    const auto managedFilePath = BuildRelativeManagedBookPath({1});
    const auto shardDirectory = BuildRelativeShardDirectory({1});
    WriteFile(sandbox.Root / managedFilePath);

    const auto bookId = repository.Add(BuildBook(managedFilePath));
    REQUIRE(bookId.IsValid());

    Librova::Application::CImportRollbackService rollback(repository, sandbox.Root);
    const auto result = rollback.RollbackImportedBooks({bookId});

    REQUIRE(result.RemainingBookIds.empty());
    REQUIRE_FALSE(std::filesystem::exists(sandbox.Root / managedFilePath));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.Root / shardDirectory));
    REQUIRE_FALSE(std::filesystem::exists((sandbox.Root / shardDirectory).parent_path()));
    REQUIRE(std::filesystem::is_directory(sandbox.Root / "Objects"));

    repository.CloseSession();
}

TEST_CASE("Rollback removes shard residue when only Thumbs.db remains in the object directory", "[rollback]")
{
    auto sandbox = CreateRollbackSandbox("remove-shard-thumbs-db");
    Librova::BookDatabase::CSqliteBookRepository repository(sandbox.DatabasePath);

    const auto managedFilePath = BuildRelativeManagedBookPath({1});
    const auto shardDirectory = BuildRelativeShardDirectory({1});
    WriteFile(sandbox.Root / managedFilePath);
    WriteFile(sandbox.Root / shardDirectory / "Thumbs.db");

    const auto bookId = repository.Add(BuildBook(managedFilePath));
    REQUIRE(bookId.IsValid());

    Librova::Application::CImportRollbackService rollback(repository, sandbox.Root);
    const auto result = rollback.RollbackImportedBooks({bookId});

    REQUIRE(result.RemainingBookIds.empty());
    REQUIRE_FALSE(result.HasCleanupResidue);
    REQUIRE_FALSE(std::filesystem::exists(sandbox.Root / shardDirectory));
    REQUIRE_FALSE(std::filesystem::exists((sandbox.Root / shardDirectory).parent_path()));
    REQUIRE(std::filesystem::is_directory(sandbox.Root / "Objects"));

    repository.CloseSession();
}

TEST_CASE("Rollback removes only cancelled book files and leaves non-empty shard directories intact", "[rollback]")
{
    auto sandbox = CreateRollbackSandbox("preserve-non-empty-shard-dir");
    Librova::BookDatabase::CSqliteBookRepository repository(sandbox.DatabasePath);

    const auto shardDir = BuildRelativeShardDirectory({1});
    const auto managedFilePath = BuildRelativeManagedBookPath({1});
    const auto siblingManagedFilePath = shardDir / u8"0000000999.book.fb2";
    WriteFile(sandbox.Root / managedFilePath);
    WriteFile(sandbox.Root / siblingManagedFilePath);

    const auto bookId = repository.Add(BuildBook(managedFilePath));
    REQUIRE(bookId.IsValid());

    Librova::Application::CImportRollbackService rollback(repository, sandbox.Root);
    const auto result = rollback.RollbackImportedBooks({bookId});

    REQUIRE(result.RemainingBookIds.empty());
    REQUIRE_FALSE(std::filesystem::exists(sandbox.Root / managedFilePath));
    REQUIRE(std::filesystem::exists(sandbox.Root / siblingManagedFilePath));
    REQUIRE(std::filesystem::is_directory(sandbox.Root / shardDir));
    REQUIRE(std::filesystem::is_directory(sandbox.Root / "Objects"));

    repository.CloseSession();
}

TEST_CASE("Rollback removes cover object but preserves Objects top-level directory", "[rollback]")
{
    auto sandbox = CreateRollbackSandbox("preserve-objects-cover-dir");
    Librova::BookDatabase::CSqliteBookRepository repository(sandbox.DatabasePath);

    const auto managedFilePath = BuildRelativeManagedBookPath({1});
    const auto coverPath = BuildRelativeCoverPath({1}, "jpg");
    WriteFile(sandbox.Root / managedFilePath);
    WriteFile(sandbox.Root / coverPath);

    const auto bookId = repository.Add(BuildBook(managedFilePath, coverPath));
    REQUIRE(bookId.IsValid());

    Librova::Application::CImportRollbackService rollback(repository, sandbox.Root);
    const auto result = rollback.RollbackImportedBooks({bookId});

    REQUIRE(result.RemainingBookIds.empty());
    REQUIRE_FALSE(std::filesystem::exists(sandbox.Root / coverPath));
    REQUIRE(std::filesystem::is_directory(sandbox.Root / "Objects"));

    repository.CloseSession();
}

TEST_CASE("Rollback service invokes progress callback at rollback start and on completion", "[rollback]")
{
    auto sandbox = CreateRollbackSandbox("progress-callback");
    Librova::BookDatabase::CSqliteBookRepository repository(sandbox.DatabasePath);

    std::vector<Librova::Domain::SBookId> bookIds;
    for (int i = 0; i < 3; ++i)
    {
        const auto managedFilePath = BuildRelativeManagedBookPath({i + 1});
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
    std::vector<std::size_t> reportedProgress;

    Librova::Application::CImportRollbackService rollback(repository, sandbox.Root);
    const auto result = rollback.RollbackImportedBooks(
        bookIds,
        [&](std::size_t rolledBack, std::size_t total)
        {
            lastRolledBack = rolledBack;
            lastTotal = total;
            ++callbackCount;
            reportedProgress.push_back(rolledBack);
        });

    REQUIRE(result.RemainingBookIds.empty());
    // Callback must publish immediate forward progress and completion.
    REQUIRE(callbackCount >= 2);
    REQUIRE(reportedProgress.front() == 1);
    // Final call must report all books rolled back.
    REQUIRE(lastRolledBack == 3);
    REQUIRE(lastTotal == 3);

    repository.CloseSession();
}

TEST_CASE("Rollback service writes phase timing summary into host log", "[rollback][logging]")
{
    auto sandbox = CreateRollbackSandbox("perf-summary");
    const auto logPath = sandbox.Root / "Logs" / "host.log";
    std::filesystem::create_directories(logPath.parent_path());
    Librova::Logging::CLogging::InitializeHostLogger(logPath);

    {
        Librova::BookDatabase::CSqliteBookRepository repository(sandbox.DatabasePath);

        std::vector<Librova::Domain::SBookId> bookIds;
        for (int i = 0; i < 2; ++i)
        {
            const auto managedFilePath = BuildRelativeManagedBookPath({i + 1});
            WriteFile(sandbox.Root / managedFilePath);

            Librova::Domain::SBook book = BuildBook(managedFilePath);
            book.File.Sha256Hex = "perf-sha-" + std::to_string(i);
            const auto id = repository.Add(book);
            REQUIRE(id.IsValid());
            bookIds.push_back(id);
        }

        Librova::Application::CImportRollbackService rollback(repository, sandbox.Root);
        const auto result = rollback.RollbackImportedBooks(bookIds);
        REQUIRE(result.RemainingBookIds.empty());

        repository.CloseSession();
    }

    Librova::Logging::CLogging::Shutdown();

    const auto logText = ReadTextFile(logPath);
    REQUIRE(logText.find("[rollback-perf] SUMMARY total=2") != std::string::npos);
    REQUIRE(logText.find("lookup=") != std::string::npos);
    REQUIRE(logText.find("batch_delete=") != std::string::npos);
    REQUIRE(logText.find("cover_cleanup=") != std::string::npos);
    REQUIRE(logText.find("book_cleanup=") != std::string::npos);
    REQUIRE(logText.find("directory_cleanup=") != std::string::npos);
    REQUIRE(logText.find("bottleneck:") != std::string::npos);
}

