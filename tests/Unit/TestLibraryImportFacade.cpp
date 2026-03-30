#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stop_token>

#include "Application/LibraryImportFacade.hpp"

namespace {

class CTestProgressSink final : public LibriFlow::Domain::IProgressSink
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

class CStubSingleFileImporter final : public LibriFlow::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] LibriFlow::Importing::SSingleFileImportResult Run(
        const LibriFlow::Importing::SSingleFileImportRequest& request,
        LibriFlow::Domain::IProgressSink&,
        std::stop_token) const override
    {
        LastRequest = request;
        return Result;
    }

    mutable std::optional<LibriFlow::Importing::SSingleFileImportRequest> LastRequest;
    LibriFlow::Importing::SSingleFileImportResult Result;
};

} // namespace

TEST_CASE("Library import facade routes regular files into single-file importer", "[application][import]")
{
    CStubSingleFileImporter importer;
    importer.Result = {
        .Status = LibriFlow::Importing::ESingleFileImportStatus::Imported,
        .ImportedBookId = LibriFlow::Domain::SBookId{7}
    };
    LibriFlow::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;

    const LibriFlow::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    const auto result = facade.Run({
        .SourcePath = "C:/books/book.fb2",
        .WorkingDirectory = "C:/work",
        .Sha256Hex = std::string{"hash"},
        .AllowProbableDuplicates = true
    }, progressSink, {});

    REQUIRE(result.Summary.Mode == LibriFlow::Application::EImportMode::SingleFile);
    REQUIRE(result.Summary.TotalEntries == 1);
    REQUIRE(result.Summary.ImportedEntries == 1);
    REQUIRE(result.IsSuccess());
    REQUIRE(importer.LastRequest.has_value());
    REQUIRE(importer.LastRequest->SourcePath == std::filesystem::path("C:/books/book.fb2"));
    REQUIRE(importer.LastRequest->Sha256Hex == std::optional<std::string>{"hash"});
    REQUIRE(importer.LastRequest->AllowProbableDuplicates);
}

TEST_CASE("Library import facade aggregates single-file failures into summary warnings", "[application][import]")
{
    CStubSingleFileImporter importer;
    importer.Result = {
        .Status = LibriFlow::Importing::ESingleFileImportStatus::Failed,
        .Warnings = {"warn"},
        .Error = "broken"
    };
    LibriFlow::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;

    const LibriFlow::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    const auto result = facade.Run({
        .SourcePath = "C:/books/book.fb2",
        .WorkingDirectory = "C:/work"
    }, progressSink, {});

    REQUIRE_FALSE(result.IsSuccess());
    REQUIRE(result.Summary.FailedEntries == 1);
    REQUIRE(result.Summary.Warnings == std::vector<std::string>({"warn", "broken"}));
}
