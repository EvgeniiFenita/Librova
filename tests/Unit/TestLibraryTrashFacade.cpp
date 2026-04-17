#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

#include "Application/LibraryTrashFacade.hpp"
#include "BookDatabase/SqliteBookRepository.hpp"
#include "DatabaseRuntime/SchemaMigrator.hpp"
#include "Logging/Logging.hpp"
#include "ManagedTrash/ManagedTrashService.hpp"

namespace {

void CloseRepositoryAndRemoveAll(
    const std::filesystem::path& path,
    Librova::BookDatabase::CSqliteBookRepository& repository)
{
    repository.CloseSession();
    std::filesystem::remove_all(path);
}

bool TryCreateDirectorySymlink(const std::filesystem::path& target, const std::filesystem::path& linkPath)
{
    std::error_code errorCode;
    std::filesystem::create_directory_symlink(target, linkPath, errorCode);
    return !errorCode;
}

class CFakeBookRepository final : public Librova::Domain::IBookRepository
{
public:
    explicit CFakeBookRepository(Librova::Domain::SBook book)
        : m_book(std::move(book))
    {
    }

    [[nodiscard]] Librova::Domain::SBookId ReserveId() override
    {
        return m_book ? m_book->Id : Librova::Domain::SBookId{};
    }

    [[nodiscard]] Librova::Domain::SBookId Add(const Librova::Domain::SBook& book) override
    {
        m_book = book;
        return book.Id;
    }

    [[nodiscard]] Librova::Domain::SBookId ForceAdd(const Librova::Domain::SBook& book) override
    {
        return Add(book);
    }

    [[nodiscard]] std::optional<Librova::Domain::SBook> GetById(const Librova::Domain::SBookId id) const override
    {
        if (m_book.has_value() && m_book->Id.Value == id.Value)
        {
            return m_book;
        }

        return std::nullopt;
    }

    void Remove(const Librova::Domain::SBookId id) override
    {
        ++RemoveCalls;
        if (ThrowOnRemove)
        {
            throw std::runtime_error("repository remove failed");
        }

        if (m_book.has_value() && m_book->Id.Value == id.Value)
        {
            m_book.reset();
        }
    }

    bool ThrowOnRemove = false;
    int RemoveCalls = 0;

private:
    std::optional<Librova::Domain::SBook> m_book;
};

class CFakeTrashService final : public Librova::Domain::ITrashService
{
public:
    explicit CFakeTrashService(std::filesystem::path trashRoot)
        : m_trashRoot(std::move(trashRoot))
    {
        std::filesystem::create_directories(m_trashRoot);
    }

    [[nodiscard]] std::filesystem::path MoveToTrash(const std::filesystem::path& path) override
    {
        ++MoveCalls;
        if (FailOnMoveCall.has_value() && *FailOnMoveCall == MoveCalls)
        {
            throw std::runtime_error("trash move failed");
        }

        const auto destination = m_trashRoot / path.filename();
        std::filesystem::create_directories(destination.parent_path());
        std::filesystem::rename(path, destination);
        return destination;
    }

    void RestoreFromTrash(
        const std::filesystem::path& trashedPath,
        const std::filesystem::path& destinationPath) override
    {
        ++RestoreCalls;
        if (FailOnRestoreCall.has_value() && *FailOnRestoreCall == RestoreCalls)
        {
            throw std::runtime_error("trash restore failed");
        }

        RestoreSequence.emplace_back(destinationPath.filename().string());
        std::filesystem::create_directories(destinationPath.parent_path());
        std::filesystem::rename(trashedPath, destinationPath);
    }

    std::optional<int> FailOnMoveCall;
    std::optional<int> FailOnRestoreCall;
    int MoveCalls = 0;
    int RestoreCalls = 0;
    std::vector<std::string> RestoreSequence;

private:
    std::filesystem::path m_trashRoot;
};

class CFakeRecycleBinService final : public Librova::Domain::IRecycleBinService
{
public:
    explicit CFakeRecycleBinService(std::filesystem::path recycleBinRoot)
        : m_recycleBinRoot(std::move(recycleBinRoot))
    {
        std::filesystem::create_directories(m_recycleBinRoot);
    }

    void MoveToRecycleBin(const std::vector<std::filesystem::path>& paths) override
    {
        ++MoveCalls;
        LastMovedPaths = paths;

        if (ThrowOnMove)
        {
            throw std::runtime_error("recycle bin move failed");
        }

        for (const auto& path : paths)
        {
            const auto destination = m_recycleBinRoot / path.filename();
            std::filesystem::create_directories(destination.parent_path());
            std::filesystem::rename(path, destination);
        }
    }

    bool ThrowOnMove = false;
    int MoveCalls = 0;
    std::vector<std::filesystem::path> LastMovedPaths;

private:
    std::filesystem::path m_recycleBinRoot;
};

} // namespace

TEST_CASE("Library trash facade keeps deleted files in managed trash when no recycle-bin service is configured", "[application][trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-trash-facade";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/5a/68");

    const auto databasePath = sandbox / "library.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    Librova::ManagedTrash::CManagedTrashService trashService(sandbox / "Library");

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Trash Me";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Objects/5a/68/0000000001.book.epub";
    book.File.SizeBytes = 12;
    book.File.Sha256Hex = "trash-hash";
    book.CoverPath = std::filesystem::path{"Objects/5a/68/0000000001.cover.jpg"};
    book.AddedAtUtc = std::chrono::system_clock::now();
    const auto bookId = repository.Add(book);

    std::ofstream(sandbox / "Library/Objects/5a/68/0000000001.book.epub", std::ios::binary) << "epub";
    std::ofstream(sandbox / "Library/Objects/5a/68/0000000001.cover.jpg", std::ios::binary) << "cover";

    const Librova::Application::CLibraryTrashFacade facade(repository, trashService, sandbox / "Library");
    const auto result = facade.MoveBookToTrash(bookId);
    const auto expectedTrashedBookPath = sandbox / "Library/Trash/Objects/5a/68/0000000001.book.epub";
    const auto expectedTrashedCoverPath = sandbox / "Library/Trash/Objects/5a/68/0000000001.cover.jpg";

    REQUIRE(result.has_value());
    REQUIRE(result->BookId.Value == bookId.Value);
    REQUIRE(result->Destination == Librova::Application::ETrashDestination::ManagedTrash);
    REQUIRE_FALSE(result->HasOrphanedFiles);
    REQUIRE(std::filesystem::exists(expectedTrashedBookPath));
    REQUIRE(std::filesystem::exists(expectedTrashedCoverPath));
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "Library/Objects/5a/68/0000000001.book.epub"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "Library/Objects/5a/68/0000000001.cover.jpg"));
    REQUIRE_FALSE(repository.GetById(bookId).has_value());

    CloseRepositoryAndRemoveAll(sandbox, repository);
}

TEST_CASE("Library trash facade finalizes staged delete into recycle bin when Windows handoff succeeds", "[application][trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-trash-facade-recycle-bin";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/7b/60");

    const auto databasePath = sandbox / "library-recycle.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    Librova::ManagedTrash::CManagedTrashService trashService(sandbox / "Library");
    CFakeRecycleBinService recycleBinService(sandbox / "RecycleBin");

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Recycle Me";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Objects/7b/60/0000000006.book.epub";
    book.File.SizeBytes = 12;
    book.File.Sha256Hex = "recycle-hash";
    book.CoverPath = std::filesystem::path{"Objects/7b/60/0000000006.cover.jpg"};
    book.AddedAtUtc = std::chrono::system_clock::now();
    const auto bookId = repository.Add(book);

    std::ofstream(sandbox / "Library/Objects/7b/60/0000000006.book.epub", std::ios::binary) << "epub";
    std::ofstream(sandbox / "Library/Objects/7b/60/0000000006.cover.jpg", std::ios::binary) << "cover";

    const Librova::Application::CLibraryTrashFacade facade(
        repository,
        trashService,
        sandbox / "Library",
        &recycleBinService);
    const auto result = facade.MoveBookToTrash(bookId);

    REQUIRE(result.has_value());
    REQUIRE(result->Destination == Librova::Application::ETrashDestination::RecycleBin);
    REQUIRE_FALSE(result->HasOrphanedFiles);
    REQUIRE(recycleBinService.MoveCalls == 1);
    REQUIRE(recycleBinService.LastMovedPaths.size() == 2);
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "Library/Trash/Objects/7b/60/0000000006.book.epub"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "Library/Trash/Objects/7b/60/0000000006.cover.jpg"));
    REQUIRE(std::filesystem::exists(sandbox / "RecycleBin/0000000006.book.epub"));
    REQUIRE(std::filesystem::exists(sandbox / "RecycleBin/0000000006.cover.jpg"));
    REQUIRE_FALSE(repository.GetById(bookId).has_value());

    CloseRepositoryAndRemoveAll(sandbox, repository);
}

TEST_CASE("Library trash facade falls back to managed trash when recycle-bin handoff fails", "[application][trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-trash-facade-recycle-fallback";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/e8/5e");

    const auto databasePath = sandbox / "library-recycle-fallback.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    Librova::ManagedTrash::CManagedTrashService trashService(sandbox / "Library");
    CFakeRecycleBinService recycleBinService(sandbox / "RecycleBin");
    recycleBinService.ThrowOnMove = true;

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Fallback";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Objects/e8/5e/0000000007.book.epub";
    book.File.SizeBytes = 12;
    book.File.Sha256Hex = "fallback-hash";
    book.AddedAtUtc = std::chrono::system_clock::now();
    const auto bookId = repository.Add(book);

    std::ofstream(sandbox / "Library/Objects/e8/5e/0000000007.book.epub", std::ios::binary) << "epub";

    const Librova::Application::CLibraryTrashFacade facade(
        repository,
        trashService,
        sandbox / "Library",
        &recycleBinService);
    const auto result = facade.MoveBookToTrash(bookId);

    REQUIRE(result.has_value());
    REQUIRE(result->Destination == Librova::Application::ETrashDestination::ManagedTrash);
    REQUIRE_FALSE(result->HasOrphanedFiles);
    REQUIRE(recycleBinService.MoveCalls == 1);
    REQUIRE(std::filesystem::exists(sandbox / "Library/Trash/Objects/e8/5e/0000000007.book.epub"));
    REQUIRE_FALSE(repository.GetById(bookId).has_value());

    CloseRepositoryAndRemoveAll(sandbox, repository);
}

TEST_CASE("Library trash facade reports orphaned files when recycle-bin handoff succeeds after partial staging", "[application][trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-trash-facade-recycle-partial";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/64/58");

    const auto bookPath = sandbox / "Library/Objects/64/58/0000000010.book.epub";
    const auto coverPath = sandbox / "Library/Objects/64/58/0000000010.cover.jpg";
    std::ofstream(bookPath, std::ios::binary) << "epub";
    std::ofstream(coverPath, std::ios::binary) << "cover";

    Librova::Domain::SBook book;
    book.Id = {10};
    book.Metadata.TitleUtf8 = "Recycle Partial";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Objects/64/58/0000000010.book.epub";
    book.File.SizeBytes = 12;
    book.CoverPath = std::filesystem::path{"Objects/64/58/0000000010.cover.jpg"};

    CFakeBookRepository repository(book);
    CFakeTrashService trashService(sandbox / "Trash");
    trashService.FailOnMoveCall = 2;
    CFakeRecycleBinService recycleBinService(sandbox / "RecycleBin");

    const Librova::Application::CLibraryTrashFacade facade(
        repository,
        trashService,
        sandbox / "Library",
        &recycleBinService);
    const auto result = facade.MoveBookToTrash(book.Id);

    REQUIRE(result.has_value());
    REQUIRE(result->Destination == Librova::Application::ETrashDestination::RecycleBin);
    REQUIRE(result->HasOrphanedFiles);
    REQUIRE(recycleBinService.MoveCalls == 1);
    REQUIRE(recycleBinService.LastMovedPaths.size() == 1);
    REQUIRE_FALSE(repository.GetById(book.Id).has_value());
    REQUIRE_FALSE(std::filesystem::exists(bookPath));
    REQUIRE(std::filesystem::exists(coverPath));
    REQUIRE(std::filesystem::exists(sandbox / "RecycleBin/0000000010.book.epub"));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library trash facade rejects symlinked managed book path escaping library root", "[application][trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-trash-facade-symlink";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/c7");
    std::filesystem::create_directories(sandbox / "Outside");

    std::ofstream(sandbox / "Outside/book.epub", std::ios::binary) << "epub";
    if (!TryCreateDirectorySymlink(sandbox / "Outside", sandbox / "Library/Objects/c7/66"))
    {
        std::filesystem::remove_all(sandbox);
        SKIP("Directory symlinks are not available in this environment.");
    }

    const auto databasePath = sandbox / "library-symlink.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    Librova::ManagedTrash::CManagedTrashService trashService(sandbox / "Library");

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Unsafe";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Objects/c7/66/0000000002.book.epub";
    book.File.SizeBytes = 12;
    book.AddedAtUtc = std::chrono::system_clock::now();
    const auto bookId = repository.Add(book);

    const Librova::Application::CLibraryTrashFacade facade(repository, trashService, sandbox / "Library");
    try
    {
        (void)facade.MoveBookToTrash(bookId);
        FAIL("Expected trash facade to reject symlinked managed path.");
    }
    catch (const std::exception& error)
    {
        REQUIRE(std::string{error.what()}.find("unsafe") != std::string::npos);
    }

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library trash facade orphans cover and keeps book in trash when cover move fails after DB removal", "[application][trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-trash-facade-rollback-cover";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/34/65");

    const auto bookPath = sandbox / "Library/Objects/34/65/0000000003.book.epub";
    const auto coverPath = sandbox / "Library/Objects/34/65/0000000003.cover.jpg";
    std::ofstream(bookPath, std::ios::binary) << "epub";
    std::ofstream(coverPath, std::ios::binary) << "cover";

    Librova::Domain::SBook book;
    book.Id = {3};
    book.Metadata.TitleUtf8 = "Rollback";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Objects/34/65/0000000003.book.epub";
    book.File.SizeBytes = 12;
    book.CoverPath = std::filesystem::path{"Objects/34/65/0000000003.cover.jpg"};

    CFakeBookRepository repository(book);
    CFakeTrashService trashService(sandbox / "Trash");
    trashService.FailOnMoveCall = 2;

    const Librova::Application::CLibraryTrashFacade facade(repository, trashService, sandbox / "Library");
    const auto result = facade.MoveBookToTrash(book.Id);

    // DB removal committed before file moves — operation succeeds even if cover move fails.
    REQUIRE(result.has_value());
    REQUIRE(result->Destination == Librova::Application::ETrashDestination::ManagedTrash);
    REQUIRE(result->HasOrphanedFiles);
    REQUIRE_FALSE(repository.GetById(book.Id).has_value());
    // Book was successfully moved to Trash.
    REQUIRE_FALSE(std::filesystem::exists(bookPath));
    // Cover move failed — file is orphaned at original location.
    REQUIRE(std::filesystem::exists(coverPath));
    // No rollback restores.
    REQUIRE(trashService.RestoreCalls == 0);

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library trash facade propagates DB remove failure without moving any files", "[application][trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-trash-facade-rollback-remove";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/a1/63");

    const auto bookPath = sandbox / "Library/Objects/a1/63/0000000004.book.epub";
    const auto coverPath = sandbox / "Library/Objects/a1/63/0000000004.cover.jpg";
    std::ofstream(bookPath, std::ios::binary) << "epub";
    std::ofstream(coverPath, std::ios::binary) << "cover";

    Librova::Domain::SBook book;
    book.Id = {4};
    book.Metadata.TitleUtf8 = "Rollback Remove";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Objects/a1/63/0000000004.book.epub";
    book.File.SizeBytes = 12;
    book.CoverPath = std::filesystem::path{"Objects/a1/63/0000000004.cover.jpg"};

    CFakeBookRepository repository(book);
    repository.ThrowOnRemove = true;
    CFakeTrashService trashService(sandbox / "Trash");

    const Librova::Application::CLibraryTrashFacade facade(repository, trashService, sandbox / "Library");

    try
    {
        (void)facade.MoveBookToTrash(book.Id);
        FAIL("Expected trash facade to surface repository remove failure.");
    }
    catch (const std::exception& error)
    {
        REQUIRE(std::string{error.what()} == "repository remove failed");
    }
    REQUIRE(std::filesystem::exists(bookPath));
    REQUIRE(std::filesystem::exists(coverPath));
    REQUIRE(repository.GetById(book.Id).has_value());
    REQUIRE(repository.RemoveCalls == 1);
    // DB remove happens first — no file moves before the throw.
    REQUIRE(trashService.MoveCalls == 0);

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library trash facade logs orphaned file warning when book move fails after DB removal", "[application][trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-trash-facade-orphan-log";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/0e/62");

    const auto bookPath = sandbox / "Library/Objects/0e/62/0000000005.book.epub";
    const auto logPath = sandbox / "Logs" / "host.log";
    std::ofstream(bookPath, std::ios::binary) << "epub";

    Librova::Domain::SBook book;
    book.Id = {5};
    book.Metadata.TitleUtf8 = "Orphan Log";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Objects/0e/62/0000000005.book.epub";
    book.File.SizeBytes = 12;

    CFakeBookRepository repository(book);
    CFakeTrashService trashService(sandbox / "Trash");
    trashService.FailOnMoveCall = 1;

    Librova::Logging::CLogging::InitializeHostLogger(logPath);
    try
    {
        const Librova::Application::CLibraryTrashFacade facade(repository, trashService, sandbox / "Library");
        const auto result = facade.MoveBookToTrash(book.Id);
        // Operation succeeds (DB already removed) even though book move failed.
        REQUIRE(result.has_value());
        REQUIRE_FALSE(repository.GetById(book.Id).has_value());
    }
    catch (...)
    {
        Librova::Logging::CLogging::Shutdown();
        throw;
    }
    Librova::Logging::CLogging::Shutdown();

    {
        std::ifstream logStream(logPath, std::ios::binary);
        const std::string logText{std::istreambuf_iterator<char>{logStream}, std::istreambuf_iterator<char>{}};
        REQUIRE(logText.find("orphan") != std::string::npos);
        REQUIRE(logText.find("trash move failed") != std::string::npos);
    }

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library trash facade removes a damaged catalog entry when the managed book file is already missing", "[application][trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-trash-facade-missing-book";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/f7/59");

    const auto coverPath = sandbox / "Library/Objects/f7/59/0000000011.cover.jpg";
    std::ofstream(coverPath, std::ios::binary) << "cover";

    Librova::Domain::SBook book;
    book.Id = {11};
    book.Metadata.TitleUtf8 = "Missing Book";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Objects/f7/59/0000000011.book.epub";
    book.File.SizeBytes = 12;
    book.CoverPath = std::filesystem::path{"Objects/f7/59/0000000011.cover.jpg"};

    CFakeBookRepository repository(book);
    CFakeTrashService trashService(sandbox / "Trash");

    const Librova::Application::CLibraryTrashFacade facade(repository, trashService, sandbox / "Library");
    const auto result = facade.MoveBookToTrash(book.Id);

    REQUIRE(result.has_value());
    REQUIRE(result->Destination == Librova::Application::ETrashDestination::ManagedTrash);
    REQUIRE(result->HasOrphanedFiles);
    REQUIRE_FALSE(repository.GetById(book.Id).has_value());
    REQUIRE_FALSE(std::filesystem::exists(coverPath));
    REQUIRE(std::filesystem::exists(sandbox / "Trash/0000000011.cover.jpg"));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library trash facade removes a damaged catalog entry when the managed cover file is already missing", "[application][trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-trash-facade-missing-cover";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/8a/5b");

    const auto bookPath = sandbox / "Library/Objects/8a/5b/0000000012.book.epub";
    std::ofstream(bookPath, std::ios::binary) << "epub";

    Librova::Domain::SBook book;
    book.Id = {12};
    book.Metadata.TitleUtf8 = "Missing Cover";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Objects/8a/5b/0000000012.book.epub";
    book.File.SizeBytes = 12;
    book.CoverPath = std::filesystem::path{"Objects/8a/5b/0000000012.cover.jpg"};

    CFakeBookRepository repository(book);
    CFakeTrashService trashService(sandbox / "Trash");

    const Librova::Application::CLibraryTrashFacade facade(repository, trashService, sandbox / "Library");
    const auto result = facade.MoveBookToTrash(book.Id);

    REQUIRE(result.has_value());
    REQUIRE(result->Destination == Librova::Application::ETrashDestination::ManagedTrash);
    REQUIRE(result->HasOrphanedFiles);
    REQUIRE_FALSE(repository.GetById(book.Id).has_value());
    REQUIRE_FALSE(std::filesystem::exists(bookPath));
    REQUIRE(std::filesystem::exists(sandbox / "Trash/0000000012.book.epub"));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library trash facade removes catalog entry before moving files so crash leaves catalog consistent", "[application][trash]")
{
    // Regression test for crash-safe delete ordering (#46).
    // Simulates: DB remove succeeds, then book move throws. The catalog must already
    // be consistent (book removed) so a crash at that point does not leave a catalog
    // entry pointing at a missing managed file.
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-trash-facade-catalog-first";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/c2/5b");

    std::ofstream(sandbox / "Library/Objects/c2/5b/0000000009.book.epub", std::ios::binary) << "epub";

    Librova::Domain::SBook book;
    book.Id = {9};
    book.Metadata.TitleUtf8 = "Catalog First";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Objects/c2/5b/0000000009.book.epub";
    book.File.SizeBytes = 12;

    CFakeBookRepository repository(book);
    CFakeTrashService trashService(sandbox / "Trash");
    trashService.FailOnMoveCall = 1;

    const Librova::Application::CLibraryTrashFacade facade(repository, trashService, sandbox / "Library");
    const auto result = facade.MoveBookToTrash(book.Id);

    // Catalog is consistent: record is gone even though the file move failed.
    REQUIRE(result.has_value());
    REQUIRE(result->HasOrphanedFiles);
    REQUIRE_FALSE(repository.GetById(book.Id).has_value());
    REQUIRE(repository.RemoveCalls == 1);
    // File is orphaned (move failed), but catalog integrity is preserved.
    REQUIRE(std::filesystem::exists(sandbox / "Library/Objects/c2/5b/0000000009.book.epub"));

    std::filesystem::remove_all(sandbox);
}
