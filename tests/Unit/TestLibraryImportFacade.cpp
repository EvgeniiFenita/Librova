#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <optional>
#include <stop_token>

#include "Application/LibraryImportFacade.hpp"

namespace {

class CTestProgressSink final : public Librova::Domain::IProgressSink
{
public:
    void ReportValue(int, std::string_view) override
    {
    }

    bool IsCancellationRequested() const override
    {
        return false;
    }
};

class CStubSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest& request,
        Librova::Domain::IProgressSink&,
        std::stop_token) const override
    {
        LastRequest = request;
        return Result;
    }

    mutable std::optional<Librova::Importing::SSingleFileImportRequest> LastRequest;
    Librova::Importing::SSingleFileImportResult Result;
};

struct SImportSandbox
{
    std::filesystem::path Root;
    std::filesystem::path SourcePath;
    std::filesystem::path WorkingDirectory;
};

SImportSandbox CreateImportSandbox(const std::string_view scenario, const std::string_view fileName = "book.fb2")
{
    const auto root = std::filesystem::temp_directory_path() / ("librova-import-facade-" + std::string{scenario});
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto sourcePath = root / fileName;
    std::ofstream(sourcePath).put('x');
    return {
        .Root = root,
        .SourcePath = sourcePath,
        .WorkingDirectory = root / "work"
    };
}

} // namespace

TEST_CASE("Library import facade routes regular files into single-file importer", "[application][import]")
{
    CStubSingleFileImporter importer;
    importer.Result = {
        .Status = Librova::Importing::ESingleFileImportStatus::Imported,
        .ImportedBookId = Librova::Domain::SBookId{7}
    };
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;
    const auto sandbox = CreateImportSandbox("single-file");

    const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    const auto result = facade.Run({
        .SourcePaths = {sandbox.SourcePath},
        .WorkingDirectory = sandbox.WorkingDirectory,
        .Sha256Hex = std::string{"hash"},
        .AllowProbableDuplicates = true
    }, progressSink, {});

    REQUIRE(result.Summary.Mode == Librova::Application::EImportMode::SingleFile);
    REQUIRE(result.Summary.TotalEntries == 1);
    REQUIRE(result.Summary.ImportedEntries == 1);
    REQUIRE(result.IsSuccess());
    REQUIRE(importer.LastRequest.has_value());
    REQUIRE(importer.LastRequest->SourcePath == sandbox.SourcePath);
    REQUIRE(importer.LastRequest->Sha256Hex == std::optional<std::string>{"hash"});
    REQUIRE(importer.LastRequest->AllowProbableDuplicates);
    std::filesystem::remove_all(sandbox.Root);
}

TEST_CASE("Library import facade aggregates single-file failures into summary warnings", "[application][import]")
{
    CStubSingleFileImporter importer;
    importer.Result = {
        .Status = Librova::Importing::ESingleFileImportStatus::Failed,
        .Warnings = {"warn"},
        .Error = "broken"
    };
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;
    const auto sandbox = CreateImportSandbox("single-file-failure");

    const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    const auto result = facade.Run({
        .SourcePaths = {sandbox.SourcePath},
        .WorkingDirectory = sandbox.WorkingDirectory
    }, progressSink, {});

    REQUIRE_FALSE(result.IsSuccess());
    REQUIRE(result.Summary.FailedEntries == 1);
    REQUIRE(result.Summary.Warnings == std::vector<std::string>({"warn", "broken"}));
    std::filesystem::remove_all(sandbox.Root);
}

TEST_CASE("Library import facade batches multiple supported files", "[application][import]")
{
    CStubSingleFileImporter importer;
    importer.Result = {
        .Status = Librova::Importing::ESingleFileImportStatus::Imported,
        .ImportedBookId = Librova::Domain::SBookId{7}
    };
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-batch";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);
    std::ofstream(sandbox / "one.fb2").put('a');
    std::ofstream(sandbox / "two.epub").put('b');

    const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    const auto result = facade.Run({
        .SourcePaths = {sandbox / "one.fb2", sandbox / "two.epub"},
        .WorkingDirectory = sandbox / "work"
    }, progressSink, {});

    REQUIRE(result.Summary.Mode == Librova::Application::EImportMode::Batch);
    REQUIRE(result.Summary.TotalEntries == 2);
    REQUIRE(result.Summary.ImportedEntries == 2);
    REQUIRE(result.Summary.FailedEntries == 0);
    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade expands directories recursively", "[application][import]")
{
    CStubSingleFileImporter importer;
    importer.Result = {
        .Status = Librova::Importing::ESingleFileImportStatus::Imported,
        .ImportedBookId = Librova::Domain::SBookId{7}
    };
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;

    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-directory";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "nested");
    std::ofstream(sandbox / "nested" / "book.fb2").put('a');
    std::ofstream(sandbox / "nested" / "book.epub").put('b');
    std::ofstream(sandbox / "nested" / "note.txt").put('c');

    const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    const auto result = facade.Run({
        .SourcePaths = {sandbox},
        .WorkingDirectory = "C:/work"
    }, progressSink, {});

    REQUIRE(result.Summary.Mode == Librova::Application::EImportMode::Batch);
    REQUIRE(result.Summary.TotalEntries == 2);
    REQUIRE(result.Summary.ImportedEntries == 2);
    REQUIRE(result.Summary.Warnings.empty());

    std::filesystem::remove_all(sandbox);
}
