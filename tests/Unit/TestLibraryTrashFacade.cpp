#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

#include "Application/LibraryTrashFacade.hpp"
#include "BookDatabase/SqliteBookRepository.hpp"
#include "DatabaseRuntime/SchemaMigrator.hpp"
#include "ManagedTrash/ManagedTrashService.hpp"

namespace {

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
        RestoreSequence.emplace_back(destinationPath.filename().string());
        std::filesystem::create_directories(destinationPath.parent_path());
        std::filesystem::rename(trashedPath, destinationPath);
    }

    std::optional<int> FailOnMoveCall;
    int MoveCalls = 0;
    int RestoreCalls = 0;
    std::vector<std::string> RestoreSequence;

private:
    std::filesystem::path m_trashRoot;
};

} // namespace

TEST_CASE("Library trash facade moves managed book and cover into library trash and removes repository row", "[application][trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-trash-facade";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Books/0000000001");
    std::filesystem::create_directories(sandbox / "Library/Covers");

    const auto databasePath = sandbox / "library.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    Librova::ManagedTrash::CManagedTrashService trashService(sandbox / "Library");

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Trash Me";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Books/0000000001/book.epub";
    book.File.SizeBytes = 12;
    book.File.Sha256Hex = "trash-hash";
    book.CoverPath = std::filesystem::path{"Covers/0000000001.jpg"};
    book.AddedAtUtc = std::chrono::system_clock::now();
    const auto bookId = repository.Add(book);

    std::ofstream(sandbox / "Library/Books/0000000001/book.epub", std::ios::binary) << "epub";
    std::ofstream(sandbox / "Library/Covers/0000000001.jpg", std::ios::binary) << "cover";

    const Librova::Application::CLibraryTrashFacade facade(repository, trashService, sandbox / "Library");
    const auto result = facade.MoveBookToTrash(bookId);

    REQUIRE(result.has_value());
    REQUIRE(result->BookId.Value == bookId.Value);
    REQUIRE(std::filesystem::exists(result->TrashedBookPath));
    REQUIRE(result->TrashedCoverPath.has_value());
    REQUIRE(std::filesystem::exists(*result->TrashedCoverPath));
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "Library/Books/0000000001/book.epub"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "Library/Covers/0000000001.jpg"));
    REQUIRE_FALSE(repository.GetById(bookId).has_value());

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library trash facade rejects symlinked managed book path escaping library root", "[application][trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-trash-facade-symlink";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Books");
    std::filesystem::create_directories(sandbox / "Outside");

    std::ofstream(sandbox / "Outside/book.epub", std::ios::binary) << "epub";
    if (!TryCreateDirectorySymlink(sandbox / "Outside", sandbox / "Library/Books/0000000002"))
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
    book.File.ManagedPath = "Books/0000000002/book.epub";
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

TEST_CASE("Library trash facade restores moved book when cover move fails", "[application][trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-trash-facade-rollback-cover";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Books/0000000003");
    std::filesystem::create_directories(sandbox / "Library/Covers");

    const auto bookPath = sandbox / "Library/Books/0000000003/book.epub";
    const auto coverPath = sandbox / "Library/Covers/0000000003.jpg";
    std::ofstream(bookPath, std::ios::binary) << "epub";
    std::ofstream(coverPath, std::ios::binary) << "cover";

    Librova::Domain::SBook book;
    book.Id = {3};
    book.Metadata.TitleUtf8 = "Rollback";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Books/0000000003/book.epub";
    book.File.SizeBytes = 12;
    book.CoverPath = std::filesystem::path{"Covers/0000000003.jpg"};

    CFakeBookRepository repository(book);
    CFakeTrashService trashService(sandbox / "Trash");
    trashService.FailOnMoveCall = 2;

    const Librova::Application::CLibraryTrashFacade facade(repository, trashService, sandbox / "Library");

    try
    {
        (void)facade.MoveBookToTrash(book.Id);
        FAIL("Expected trash facade to surface cover move failure.");
    }
    catch (const std::exception& error)
    {
        REQUIRE(std::string{error.what()} == "trash move failed");
    }
    REQUIRE(std::filesystem::exists(bookPath));
    REQUIRE(std::filesystem::exists(coverPath));
    REQUIRE(repository.GetById(book.Id).has_value());
    REQUIRE(repository.RemoveCalls == 0);
    REQUIRE(trashService.RestoreCalls == 1);

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library trash facade restores book and cover when repository remove fails", "[application][trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-trash-facade-rollback-remove";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Books/0000000004");
    std::filesystem::create_directories(sandbox / "Library/Covers");

    const auto bookPath = sandbox / "Library/Books/0000000004/book.epub";
    const auto coverPath = sandbox / "Library/Covers/0000000004.jpg";
    std::ofstream(bookPath, std::ios::binary) << "epub";
    std::ofstream(coverPath, std::ios::binary) << "cover";

    Librova::Domain::SBook book;
    book.Id = {4};
    book.Metadata.TitleUtf8 = "Rollback Remove";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Books/0000000004/book.epub";
    book.File.SizeBytes = 12;
    book.CoverPath = std::filesystem::path{"Covers/0000000004.jpg"};

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
    REQUIRE(trashService.RestoreCalls == 2);
    REQUIRE(trashService.RestoreSequence.size() == 2);
    REQUIRE(trashService.RestoreSequence[0] == "0000000004.jpg");
    REQUIRE(trashService.RestoreSequence[1] == "book.epub");

    std::filesystem::remove_all(sandbox);
}
