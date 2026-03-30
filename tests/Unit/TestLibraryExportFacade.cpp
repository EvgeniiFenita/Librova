#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "Application/LibraryExportFacade.hpp"
#include "BookDatabase/SqliteBookRepository.hpp"
#include "DatabaseRuntime/SchemaMigrator.hpp"

namespace {

class CScopedDirectory final
{
public:
    explicit CScopedDirectory(std::filesystem::path path)
        : m_path(std::move(path))
    {
        std::error_code errorCode;
        std::filesystem::remove_all(m_path, errorCode);
        std::filesystem::create_directories(m_path);
    }

    ~CScopedDirectory()
    {
        std::error_code errorCode;
        std::filesystem::remove_all(m_path, errorCode);
    }

    [[nodiscard]] const std::filesystem::path& GetPath() const noexcept
    {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

std::string ReadAllText(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

Librova::Domain::SBook CreateBook(
    const Librova::Domain::SBookId id,
    const std::filesystem::path& managedPath)
{
    Librova::Domain::SBook book;
    book.Id = id;
    book.Metadata.TitleUtf8 = "Roadside Picnic";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = managedPath;
    book.File.SizeBytes = 123;
    return book;
}

} // namespace

TEST_CASE("LibraryExportFacade exports managed book file to requested destination", "[application][export]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-library-export");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto sourcePath = libraryRoot / "Books/0000000001/book.epub";
    const auto destinationPath = sandbox.GetPath() / "Exports" / "roadside-picnic.epub";

    std::filesystem::create_directories(sourcePath.parent_path());
    std::ofstream(sourcePath, std::ios::binary) << "epub-contents";

    const auto databasePath = sandbox.GetPath() / "librova-library-export.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    const auto bookId = repository.Add(CreateBook({1}, "Books/0000000001/book.epub"));

    const Librova::Application::CLibraryExportFacade facade(repository, libraryRoot);
    const auto exportedPath = facade.ExportBook(bookId, destinationPath);

    REQUIRE(exportedPath.has_value());
    REQUIRE(*exportedPath == destinationPath);
    REQUIRE(std::filesystem::exists(destinationPath));
    REQUIRE(ReadAllText(destinationPath) == "epub-contents");
}

TEST_CASE("LibraryExportFacade rejects unsafe managed paths", "[application][export]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-library-export-unsafe");
    const auto databasePath = sandbox.GetPath() / "librova-library-export-unsafe.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    const auto bookId = repository.Add(CreateBook({1}, "../outside/book.epub"));

    const Librova::Application::CLibraryExportFacade facade(repository, sandbox.GetPath() / "Library");

    REQUIRE_THROWS(
        facade.ExportBook(bookId, sandbox.GetPath() / "Exports" / "book.epub"));
}

TEST_CASE("LibraryExportFacade accepts absolute managed path under library root", "[application][export]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-library-export-absolute");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto sourcePath = libraryRoot / "Books/0000000002/book.fb2";
    const auto destinationPath = sandbox.GetPath() / "Exports" / "book.fb2";

    std::filesystem::create_directories(sourcePath.parent_path());
    std::ofstream(sourcePath, std::ios::binary) << "fb2-contents";

    const auto databasePath = sandbox.GetPath() / "librova-library-export-absolute.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    const auto bookId = repository.Add(CreateBook({2}, sourcePath));

    const Librova::Application::CLibraryExportFacade facade(repository, libraryRoot);
    const auto exportedPath = facade.ExportBook(bookId, destinationPath);

    REQUIRE(exportedPath.has_value());
    REQUIRE(ReadAllText(destinationPath) == "fb2-contents");
}
