#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

#include "Application/LibraryTrashFacade.hpp"
#include "BookDatabase/SqliteBookRepository.hpp"
#include "DatabaseRuntime/SchemaMigrator.hpp"
#include "ManagedTrash/ManagedTrashService.hpp"

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
