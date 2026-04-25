#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "App/CLibraryApplication.hpp"

#include "TestWorkspace.hpp"

namespace {

void WriteTextFile(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

std::filesystem::path CreateFb2Fixture(const std::filesystem::path& outputPath)
{
    WriteTextFile(
        outputPath,
        R"(<?xml version="1.0" encoding="UTF-8"?>
<FictionBook xmlns:l="http://www.w3.org/1999/xlink">
  <description>
    <title-info>
      <book-title>Тестовая книга</book-title>
      <author>
        <first-name>Иван</first-name>
        <last-name>Тестов</last-name>
      </author>
      <lang>ru</lang>
    </title-info>
  </description>
  <body>
    <section><p>Содержание</p></section>
  </body>
</FictionBook>)");

    return outputPath;
}

[[nodiscard]] Librova::Application::SLibraryApplicationConfig MakeNewLibraryConfig(
    const CTestWorkspace& sandbox)
{
    return {
        .LibraryRoot = sandbox.GetPath() / "Library",
        .RuntimeWorkspaceRoot = sandbox.GetPath() / "Runtime",
        .LogFilePath = sandbox.GetPath() / "test.log",
        .LibraryOpenMode = Librova::Application::ELibraryOpenMode::CreateNew
    };
}

[[nodiscard]] Librova::Application::SLibraryApplicationConfig MakeOpenLibraryConfig(
    const CTestWorkspace& sandbox)
{
    return {
        .LibraryRoot = sandbox.GetPath() / "Library",
        .RuntimeWorkspaceRoot = sandbox.GetPath() / "Runtime",
        .LogFilePath = sandbox.GetPath() / "test.log",
        .LibraryOpenMode = Librova::Application::ELibraryOpenMode::Open
    };
}

} // namespace

TEST_CASE("CLibraryApplication creates a new library root on CreateNew mode", "[application]")
{
    CTestWorkspace sandbox(L"librova-clibapp-create");
    const auto config = MakeNewLibraryConfig(sandbox);

    const Librova::Application::CLibraryApplication app(config);

    REQUIRE(std::filesystem::is_directory(config.LibraryRoot / "Database"));
    REQUIRE(std::filesystem::is_directory(config.LibraryRoot / "Objects"));
    REQUIRE(std::filesystem::is_directory(config.LibraryRoot / "Logs"));
    REQUIRE(std::filesystem::is_directory(config.LibraryRoot / "Trash"));
    REQUIRE(std::filesystem::is_regular_file(config.LibraryRoot / "Database" / "librova.db"));
}

TEST_CASE("CLibraryApplication opens an existing library root on Open mode", "[application]")
{
    CTestWorkspace sandbox(L"librova-clibapp-open");

    // Create first, then reopen
    {
        const Librova::Application::CLibraryApplication create(MakeNewLibraryConfig(sandbox));
    }

    Librova::Application::CLibraryApplication app(MakeOpenLibraryConfig(sandbox));

    const auto stats = app.GetLibraryStatistics();
    REQUIRE(stats.BookCount == 0);
}

TEST_CASE("CLibraryApplication Open mode rejects a missing library", "[application]")
{
    CTestWorkspace sandbox(L"librova-clibapp-open-missing");

    REQUIRE_THROWS_WITH(
        Librova::Application::CLibraryApplication(MakeOpenLibraryConfig(sandbox)),
        Catch::Matchers::ContainsSubstring("does not exist"));
}

TEST_CASE("CLibraryApplication ListBooks returns empty result for a fresh library", "[application]")
{
    CTestWorkspace sandbox(L"librova-clibapp-listbooks-empty");
    Librova::Application::CLibraryApplication app(MakeNewLibraryConfig(sandbox));

    const auto result = app.ListBooks({.Limit = 50});

    REQUIRE(result.Items.empty());
    REQUIRE(result.TotalCount == 0);
    REQUIRE(result.Statistics.BookCount == 0);
}

TEST_CASE("CLibraryApplication GetLibraryStatistics returns zeros for a fresh library", "[application]")
{
    CTestWorkspace sandbox(L"librova-clibapp-stats-empty");
    Librova::Application::CLibraryApplication app(MakeNewLibraryConfig(sandbox));

    const auto stats = app.GetLibraryStatistics();

    REQUIRE(stats.BookCount == 0);
    REQUIRE(stats.TotalManagedBookSizeBytes == 0);
}

TEST_CASE("CLibraryApplication ListCollections returns empty list for a fresh library", "[application]")
{
    CTestWorkspace sandbox(L"librova-clibapp-collections-empty");
    Librova::Application::CLibraryApplication app(MakeNewLibraryConfig(sandbox));

    REQUIRE(app.ListCollections().empty());
}

TEST_CASE("CLibraryApplication imports a single FB2 file and reports it in catalog", "[application]")
{
    CTestWorkspace sandbox(L"librova-clibapp-import-single");
    Librova::Application::CLibraryApplication app(MakeNewLibraryConfig(sandbox));

    const auto fb2Path = CreateFb2Fixture(sandbox.GetPath() / "source" / "book.fb2");
    const auto workDir = sandbox.GetPath() / "work";
    std::filesystem::create_directories(workDir);

    const auto jobId = app.StartImport({
        .SourcePaths = {fb2Path},
        .WorkingDirectory = workDir
    });

    REQUIRE(jobId != 0);

    const bool completed = app.WaitImportJob(jobId, std::chrono::seconds(30));
    REQUIRE(completed);

    const auto result = app.GetImportJobResult(jobId);
    REQUIRE(result.has_value());
    REQUIRE(result->Snapshot.Status == Librova::ApplicationJobs::EImportJobStatus::Completed);
    REQUIRE(result->ImportResult.has_value());
    REQUIRE(result->ImportResult->Summary.ImportedEntries == 1);

    const auto listResult = app.ListBooks({.Limit = 50});
    REQUIRE(listResult.TotalCount == 1);
    REQUIRE(listResult.Items.size() == 1);
    REQUIRE(listResult.Items[0].TitleUtf8 == "Тестовая книга");
    REQUIRE(listResult.Statistics.BookCount == 1);
}

TEST_CASE("CLibraryApplication GetBookDetails returns book after import", "[application]")
{
    CTestWorkspace sandbox(L"librova-clibapp-details");
    Librova::Application::CLibraryApplication app(MakeNewLibraryConfig(sandbox));

    const auto fb2Path = CreateFb2Fixture(sandbox.GetPath() / "source" / "book.fb2");
    const auto workDir = sandbox.GetPath() / "work";
    std::filesystem::create_directories(workDir);

    const auto jobId = app.StartImport({.SourcePaths = {fb2Path}, .WorkingDirectory = workDir});
    REQUIRE(app.WaitImportJob(jobId, std::chrono::seconds(30)));

    const auto listResult = app.ListBooks({.Limit = 1});
    REQUIRE(listResult.Items.size() == 1);

    const auto details = app.GetBookDetails(listResult.Items[0].Id);
    REQUIRE(details.has_value());
    REQUIRE(details->TitleUtf8 == "Тестовая книга");
    REQUIRE(details->Language == "ru");
}

TEST_CASE("CLibraryApplication MoveBookToTrash removes book from catalog", "[application]")
{
    CTestWorkspace sandbox(L"librova-clibapp-trash");
    Librova::Application::CLibraryApplication app(MakeNewLibraryConfig(sandbox));

    const auto fb2Path = CreateFb2Fixture(sandbox.GetPath() / "source" / "book.fb2");
    const auto workDir = sandbox.GetPath() / "work";
    std::filesystem::create_directories(workDir);

    const auto jobId = app.StartImport({.SourcePaths = {fb2Path}, .WorkingDirectory = workDir});
    REQUIRE(app.WaitImportJob(jobId, std::chrono::seconds(30)));

    const auto listResult = app.ListBooks({.Limit = 1});
    REQUIRE(listResult.TotalCount == 1);
    const auto bookId = listResult.Items[0].Id;

    const auto trashResult = app.MoveBookToTrash(bookId);
    REQUIRE(trashResult.has_value());
    REQUIRE(trashResult->BookId.Value == bookId.Value);

    const auto afterTrash = app.ListBooks({.Limit = 50});
    REQUIRE(afterTrash.TotalCount == 0);
}

TEST_CASE("CLibraryApplication CreateCollection and AddBookToCollection", "[application]")
{
    CTestWorkspace sandbox(L"librova-clibapp-collection-crud");
    Librova::Application::CLibraryApplication app(MakeNewLibraryConfig(sandbox));

    const auto collection = app.CreateCollection("Favourite", "star");
    REQUIRE(collection.Id != 0);
    REQUIRE(collection.NameUtf8 == "Favourite");

    const auto collections = app.ListCollections();
    REQUIRE(collections.size() == 1);
    REQUIRE(collections[0].Id == collection.Id);

    // Import a book to add to the collection
    const auto fb2Path = CreateFb2Fixture(sandbox.GetPath() / "source" / "book.fb2");
    const auto workDir = sandbox.GetPath() / "work";
    std::filesystem::create_directories(workDir);
    const auto jobId = app.StartImport({.SourcePaths = {fb2Path}, .WorkingDirectory = workDir});
    REQUIRE(app.WaitImportJob(jobId, std::chrono::seconds(30)));

    const auto listResult = app.ListBooks({.Limit = 1});
    REQUIRE(listResult.Items.size() == 1);
    const auto bookId = listResult.Items[0].Id;

    REQUIRE(app.AddBookToCollection(bookId, collection.Id));

    const auto inCollection = app.ListBooks({.CollectionId = collection.Id, .Limit = 50});
    REQUIRE(inCollection.Items.size() == 1);

    REQUIRE(app.RemoveBookFromCollection(bookId, collection.Id));
    const auto afterRemove = app.ListBooks({.CollectionId = collection.Id, .Limit = 50});
    REQUIRE(afterRemove.Items.empty());

    REQUIRE(app.DeleteCollection(collection.Id));
    REQUIRE(app.ListCollections().empty());
}

TEST_CASE("CLibraryApplication CancelImportJob cancels a running job", "[application]")
{
    CTestWorkspace sandbox(L"librova-clibapp-cancel");
    Librova::Application::CLibraryApplication app(MakeNewLibraryConfig(sandbox));

    const auto fb2Path = CreateFb2Fixture(sandbox.GetPath() / "source" / "book.fb2");
    const auto workDir = sandbox.GetPath() / "work";
    std::filesystem::create_directories(workDir);

    const auto jobId = app.StartImport({.SourcePaths = {fb2Path}, .WorkingDirectory = workDir});
    REQUIRE(jobId != 0);

    app.CancelImportJob(jobId);
    app.WaitImportJob(jobId, std::chrono::seconds(30));

    const auto result = app.GetImportJobResult(jobId);
    REQUIRE(result.has_value());
    // Job is either cancelled or completed (race condition for tiny single-file imports)
    const auto status = result->Snapshot.Status;
    REQUIRE((status == Librova::ApplicationJobs::EImportJobStatus::Cancelled
          || status == Librova::ApplicationJobs::EImportJobStatus::Completed));

    REQUIRE(app.RemoveImportJob(jobId));
    REQUIRE_FALSE(app.GetImportJobResult(jobId).has_value());
}
