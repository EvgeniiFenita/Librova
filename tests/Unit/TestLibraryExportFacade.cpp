#include <catch2/catch_test_macros.hpp>
#include "TestWorkspace.hpp"
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <stop_token>

#include "App/LibraryExportFacade.hpp"
#include "Database/SqliteBookRepository.hpp"
#include "Database/SchemaMigrator.hpp"
#include "Foundation/Logging.hpp"
#include "Storage/ManagedFileEncoding.hpp"

namespace {


std::string ReadAllText(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

bool TryCreateDirectorySymlink(const std::filesystem::path& target, const std::filesystem::path& linkPath)
{
    std::error_code errorCode;
    std::filesystem::create_directory_symlink(target, linkPath, errorCode);
    return !errorCode;
}

class CStubBookConverter final : public Librova::Domain::IBookConverter
{
public:
    [[nodiscard]] bool CanConvert(
        const Librova::Domain::EBookFormat sourceFormat,
        const Librova::Domain::EBookFormat destinationFormat) const override
    {
        return Enabled
            && sourceFormat == Librova::Domain::EBookFormat::Fb2
            && destinationFormat == Librova::Domain::EBookFormat::Epub;
    }

    [[nodiscard]] Librova::Domain::SConversionResult Convert(
        const Librova::Domain::SConversionRequest& request,
        Librova::Domain::IProgressSink&,
        std::stop_token) const override
    {
        LastRequest = request;
        LastSourceText = ReadAllText(request.SourcePath);
        if (!Enabled)
        {
            return {
                .Status = Librova::Domain::EConversionStatus::Failed,
                .Warnings = {"Configured FB2 to EPUB export converter is unavailable."}
            };
        }

        if (!request.DestinationPath.parent_path().empty())
        {
            std::filesystem::create_directories(request.DestinationPath.parent_path());
        }

        std::ofstream(request.DestinationPath, std::ios::binary) << "converted-epub";
        return {
            .Status = Librova::Domain::EConversionStatus::Succeeded,
            .OutputPath = request.DestinationPath
        };
    }

    mutable std::optional<Librova::Domain::SConversionRequest> LastRequest;
    mutable std::string LastSourceText;
    bool Enabled = true;
};

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
    CTestWorkspace sandbox(L"librova-library-export");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto sourcePath = libraryRoot / "Objects/5a/68/0000000001.book.epub";
    const auto destinationPath = sandbox.GetPath() / "Exports" / "roadside-picnic.epub";

    std::filesystem::create_directories(sourcePath.parent_path());
    std::ofstream(sourcePath, std::ios::binary) << "epub-contents";

    const auto databasePath = sandbox.GetPath() / "librova-library-export.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    const auto bookId = repository.Add(CreateBook({1}, "Objects/5a/68/0000000001.book.epub"));

    const Librova::Application::CLibraryExportFacade facade(repository, libraryRoot);
    const auto exportedPath = facade.ExportBook({
        .BookId = bookId,
        .DestinationPath = destinationPath
    });

    REQUIRE(exportedPath.has_value());
    REQUIRE(*exportedPath == destinationPath);
    REQUIRE(std::filesystem::exists(destinationPath));
    REQUIRE(ReadAllText(destinationPath) == "epub-contents");
}

TEST_CASE("LibraryExportFacade rejects unsafe managed paths", "[application][export]")
{
    CTestWorkspace sandbox(L"librova-library-export-unsafe");
    const auto databasePath = sandbox.GetPath() / "librova-library-export-unsafe.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    const auto bookId = repository.Add(CreateBook({1}, "../outside/book.epub"));

    const Librova::Application::CLibraryExportFacade facade(repository, sandbox.GetPath() / "Library");

    REQUIRE_THROWS(
        facade.ExportBook({
            .BookId = bookId,
            .DestinationPath = sandbox.GetPath() / "Exports" / "book.epub"
        }));
}

TEST_CASE("LibraryExportFacade rejects export destinations inside the managed library", "[application][export]")
{
    CTestWorkspace sandbox(L"librova-library-export-inside-library");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto sourcePath = libraryRoot / "Objects/64/58/0000000010.book.epub";
    const auto destinationPath = sourcePath;

    std::filesystem::create_directories(sourcePath.parent_path());
    std::ofstream(sourcePath, std::ios::binary) << "epub-contents";

    const auto databasePath = sandbox.GetPath() / "librova-library-export-inside-library.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    const auto bookId = repository.Add(CreateBook({10}, "Objects/64/58/0000000010.book.epub"));

    const Librova::Application::CLibraryExportFacade facade(repository, libraryRoot);

    REQUIRE_THROWS_WITH(
        facade.ExportBook({
            .BookId = bookId,
            .DestinationPath = destinationPath
        }),
        Catch::Matchers::ContainsSubstring("outside the managed library"));
    REQUIRE(ReadAllText(sourcePath) == "epub-contents");
}

TEST_CASE("LibraryExportFacade accepts absolute managed path under library root", "[application][export]")
{
    CTestWorkspace sandbox(L"librova-library-export-absolute");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto sourcePath = libraryRoot / "Objects/c7/66/0000000002.book.fb2";
    const auto destinationPath = sandbox.GetPath() / "Exports" / "book.fb2";

    std::filesystem::create_directories(sourcePath.parent_path());
    std::ofstream(sourcePath, std::ios::binary) << "fb2-contents";

    const auto databasePath = sandbox.GetPath() / "librova-library-export-absolute.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    const auto bookId = repository.Add(CreateBook({2}, sourcePath));

    const Librova::Application::CLibraryExportFacade facade(repository, libraryRoot);
    const auto exportedPath = facade.ExportBook({
        .BookId = bookId,
        .DestinationPath = destinationPath
    });

    REQUIRE(exportedPath.has_value());
    REQUIRE(ReadAllText(destinationPath) == "fb2-contents");
}

TEST_CASE("LibraryExportFacade rejects symlinked managed path escaping library root", "[application][export]")
{
    CTestWorkspace sandbox(L"librova-library-export-symlink");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto outsideRoot = sandbox.GetPath() / "Outside";
    const auto destinationPath = sandbox.GetPath() / "Exports" / "book.epub";

    std::filesystem::create_directories(outsideRoot);
    std::filesystem::create_directories(libraryRoot / "Objects/00");
    std::ofstream(outsideRoot / "book.epub", std::ios::binary) << "outside-contents";

    if (!TryCreateDirectorySymlink(outsideRoot, libraryRoot / "Objects/00/00"))
    {
        SKIP("Directory symlinks are not available in this environment.");
    }

    const auto databasePath = sandbox.GetPath() / "librova-library-export-symlink.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    const auto bookId = repository.Add(CreateBook({3}, "Objects/34/65/0000000003.book.epub"));

    const Librova::Application::CLibraryExportFacade facade(repository, libraryRoot);
    try
    {
        (void)facade.ExportBook({
            .BookId = bookId,
            .DestinationPath = destinationPath
        });
        FAIL("Expected export to reject symlinked managed path.");
    }
    catch (const std::exception& error)
    {
        REQUIRE(std::string{error.what()}.find("unsafe") != std::string::npos);
    }
}

TEST_CASE("LibraryExportFacade overwrites an existing destination file", "[application][export]")
{
    CTestWorkspace sandbox(L"librova-library-export-overwrite");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto sourcePath = libraryRoot / "Objects/a1/63/0000000004.book.epub";
    const auto destinationPath = sandbox.GetPath() / "Exports" / "book.epub";
    const auto logPath = sandbox.GetPath() / "Logs" / "export.log";

    std::filesystem::create_directories(sourcePath.parent_path());
    std::filesystem::create_directories(destinationPath.parent_path());
    std::ofstream(sourcePath, std::ios::binary) << "new-contents";
    std::ofstream(destinationPath, std::ios::binary) << "old-contents";

    const auto databasePath = sandbox.GetPath() / "librova-library-export-overwrite.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    const auto bookId = repository.Add(CreateBook({4}, "Objects/a1/63/0000000004.book.epub"));

    Librova::Logging::CLogging::InitializeHostLogger(logPath);
    try
    {
        const Librova::Application::CLibraryExportFacade facade(repository, libraryRoot);
        const auto exportedPath = facade.ExportBook({
            .BookId = bookId,
            .DestinationPath = destinationPath
        });

        REQUIRE(exportedPath.has_value());
        REQUIRE(ReadAllText(destinationPath) == "new-contents");
    }
    catch (...)
    {
        Librova::Logging::CLogging::Shutdown();
        throw;
    }
    Librova::Logging::CLogging::Shutdown();

    const auto logText = ReadAllText(logPath);
    REQUIRE(logText.find("overwritten") != std::string::npos);
    REQUIRE(logText.find("Exported managed book") != std::string::npos);
}

TEST_CASE("LibraryExportFacade converts FB2 export to EPUB when converter is configured", "[application][export]")
{
    CTestWorkspace sandbox(L"librova-library-export-converted");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto sourcePath = libraryRoot / "Objects/0e/62/0000000005.book.fb2";
    const auto destinationPath = sandbox.GetPath() / "Exports" / "book.epub";

    std::filesystem::create_directories(sourcePath.parent_path());
    std::ofstream(sourcePath, std::ios::binary) << "fb2-contents";

    const auto databasePath = sandbox.GetPath() / "librova-library-export-converted.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    auto book = CreateBook({5}, "Objects/0e/62/0000000005.book.fb2");
    book.File.Format = Librova::Domain::EBookFormat::Fb2;
    const auto bookId = repository.Add(book);

    CStubBookConverter converter;
    const Librova::Application::CLibraryExportFacade facade(repository, libraryRoot, &converter);
    const auto exportedPath = facade.ExportBook({
        .BookId = bookId,
        .DestinationPath = destinationPath,
        .ExportFormat = Librova::Domain::EBookFormat::Epub
    });

    REQUIRE(exportedPath.has_value());
    REQUIRE(*exportedPath == destinationPath);
    REQUIRE(ReadAllText(destinationPath) == "converted-epub");
    REQUIRE(converter.LastRequest.has_value());
    REQUIRE(converter.LastRequest->SourcePath == sourcePath);
    REQUIRE(converter.LastRequest->DestinationPath == destinationPath);
}

TEST_CASE("LibraryExportFacade transparently decompresses managed FB2 on direct export", "[application][export]")
{
    CTestWorkspace sandbox(L"librova-library-export-compressed-fb2");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto plainSourcePath = sandbox.GetPath() / "input" / "source.fb2";
    const auto managedSourcePath = libraryRoot / "Objects/e8/5e/0000000007.book.fb2";
    const auto destinationPath = sandbox.GetPath() / "Exports" / "book.fb2";

    std::filesystem::create_directories(plainSourcePath.parent_path());
    std::filesystem::create_directories(managedSourcePath.parent_path());
    std::ofstream(plainSourcePath, std::ios::binary) << "plain-fb2-contents";
    Librova::ManagedFileEncoding::EncodeFileToPath(
        plainSourcePath,
        Librova::Domain::EStorageEncoding::Compressed,
        managedSourcePath);

    const auto databasePath = sandbox.GetPath() / "librova-library-export-compressed-fb2.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    auto book = CreateBook({7}, "Objects/e8/5e/0000000007.book.fb2");
    book.File.Format = Librova::Domain::EBookFormat::Fb2;
    book.File.StorageEncoding = Librova::Domain::EStorageEncoding::Compressed;
    const auto bookId = repository.Add(book);

    const Librova::Application::CLibraryExportFacade facade(repository, libraryRoot);
    const auto exportedPath = facade.ExportBook({
        .BookId = bookId,
        .DestinationPath = destinationPath
    });

    REQUIRE(exportedPath.has_value());
    REQUIRE(ReadAllText(destinationPath) == "plain-fb2-contents");
}

TEST_CASE("LibraryExportFacade decodes compressed managed FB2 before EPUB conversion", "[application][export]")
{
    CTestWorkspace sandbox(L"librova-library-export-compressed-converted");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto plainSourcePath = sandbox.GetPath() / "input" / "source.fb2";
    const auto managedSourcePath = libraryRoot / "Objects/55/5d/0000000008.book.fb2";
    const auto destinationPath = sandbox.GetPath() / "Exports" / "book.epub";

    std::filesystem::create_directories(plainSourcePath.parent_path());
    std::filesystem::create_directories(managedSourcePath.parent_path());
    std::ofstream(plainSourcePath, std::ios::binary) << "plain-conversion-source";
    Librova::ManagedFileEncoding::EncodeFileToPath(
        plainSourcePath,
        Librova::Domain::EStorageEncoding::Compressed,
        managedSourcePath);

    const auto databasePath = sandbox.GetPath() / "librova-library-export-compressed-converted.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    auto book = CreateBook({8}, "Objects/55/5d/0000000008.book.fb2");
    book.File.Format = Librova::Domain::EBookFormat::Fb2;
    book.File.StorageEncoding = Librova::Domain::EStorageEncoding::Compressed;
    const auto bookId = repository.Add(book);

    CStubBookConverter converter;
    const Librova::Application::CLibraryExportFacade facade(repository, libraryRoot, &converter);
    const auto exportedPath = facade.ExportBook({
        .BookId = bookId,
        .DestinationPath = destinationPath,
        .ExportFormat = Librova::Domain::EBookFormat::Epub
    });

    REQUIRE(exportedPath.has_value());
    REQUIRE(ReadAllText(destinationPath) == "converted-epub");
    REQUIRE(converter.LastRequest.has_value());
    REQUIRE(converter.LastRequest->SourcePath != managedSourcePath);
    REQUIRE(converter.LastSourceText == "plain-conversion-source");
    REQUIRE_FALSE(std::filesystem::exists(converter.LastRequest->SourcePath));
}

TEST_CASE("LibraryExportFacade rejects converted FB2 export when converter is not configured", "[application][export]")
{
    CTestWorkspace sandbox(L"librova-library-export-converted-missing");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto sourcePath = libraryRoot / "Objects/7b/60/0000000006.book.fb2";
    const auto destinationPath = sandbox.GetPath() / "Exports" / "book.epub";

    std::filesystem::create_directories(sourcePath.parent_path());
    std::ofstream(sourcePath, std::ios::binary) << "fb2-contents";

    const auto databasePath = sandbox.GetPath() / "librova-library-export-converted-missing.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    auto book = CreateBook({6}, "Objects/7b/60/0000000006.book.fb2");
    book.File.Format = Librova::Domain::EBookFormat::Fb2;
    const auto bookId = repository.Add(book);

    const Librova::Application::CLibraryExportFacade facade(repository, libraryRoot);

    REQUIRE_THROWS_WITH(
        facade.ExportBook({
            .BookId = bookId,
            .DestinationPath = destinationPath,
            .ExportFormat = Librova::Domain::EBookFormat::Epub
        }),
        Catch::Matchers::ContainsSubstring("converter is unavailable"));
}

TEST_CASE("LibraryExportFacade cleans temporary decoded FB2 when decompression fails", "[application][export]")
{
    CTestWorkspace sandbox(L"librova-library-export-broken-compressed-fb2");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto managedSourcePath = libraryRoot / "Objects/f7/59/0000000011.book.fb2";
    const auto destinationPath = sandbox.GetPath() / "Exports" / "book.epub";
    const auto legacyTempExportDirectory = libraryRoot / "Temp" / "Export";

    std::filesystem::create_directories(managedSourcePath.parent_path());
    std::ofstream(managedSourcePath, std::ios::binary) << "broken-gzip";

    const auto databasePath = sandbox.GetPath() / "librova-library-export-broken-compressed-fb2.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::BookDatabase::CSqliteBookRepository repository(databasePath);
    auto book = CreateBook({11}, "Objects/f7/59/0000000011.book.fb2");
    book.File.Format = Librova::Domain::EBookFormat::Fb2;
    book.File.StorageEncoding = Librova::Domain::EStorageEncoding::Compressed;
    const auto bookId = repository.Add(book);

    CStubBookConverter converter;
    const Librova::Application::CLibraryExportFacade facade(repository, libraryRoot, &converter);

    REQUIRE_THROWS_WITH(
        facade.ExportBook({
            .BookId = bookId,
            .DestinationPath = destinationPath,
            .ExportFormat = Librova::Domain::EBookFormat::Epub
        }),
        Catch::Matchers::ContainsSubstring("decompression"));

    REQUIRE_FALSE(std::filesystem::exists(destinationPath));
    REQUIRE_FALSE(std::filesystem::exists(legacyTempExportDirectory));
    if (std::filesystem::exists(legacyTempExportDirectory))
    {
        REQUIRE(std::filesystem::is_empty(legacyTempExportDirectory));
    }
}

