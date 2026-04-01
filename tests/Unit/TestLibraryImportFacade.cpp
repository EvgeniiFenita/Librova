#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include <zip.h>

#include "Application/LibraryImportFacade.hpp"
#include "Logging/Logging.hpp"

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

class CStructuredProgressSink final : public Librova::Domain::IProgressSink, public Librova::Domain::IStructuredImportProgressSink
{
public:
    void ReportValue(int, std::string_view) override
    {
    }

    bool IsCancellationRequested() const override
    {
        return false;
    }

    void ReportStructuredProgress(
        const std::size_t totalEntries,
        const std::size_t processedEntries,
        const std::size_t importedEntries,
        const std::size_t failedEntries,
        const std::size_t skippedEntries,
        const int percent,
        std::string_view message) override
    {
        Snapshots.push_back({
            .TotalEntries = totalEntries,
            .ProcessedEntries = processedEntries,
            .ImportedEntries = importedEntries,
            .FailedEntries = failedEntries,
            .SkippedEntries = skippedEntries,
            .Percent = percent,
            .Message = std::string{message}
        });
    }

    struct SSnapshot
    {
        std::size_t TotalEntries = 0;
        std::size_t ProcessedEntries = 0;
        std::size_t ImportedEntries = 0;
        std::size_t FailedEntries = 0;
        std::size_t SkippedEntries = 0;
        int Percent = 0;
        std::string Message;
    };

    std::vector<SSnapshot> Snapshots;
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

void AddZipEntry(zip_t* archive, const std::string& entryPath, const std::string& text)
{
    void* buffer = std::malloc(text.size());

    if (buffer == nullptr)
    {
        throw std::runtime_error("Failed to allocate zip buffer.");
    }

    std::memcpy(buffer, text.data(), text.size());
    zip_source_t* source = zip_source_buffer(archive, buffer, text.size(), 1);

    if (source == nullptr)
    {
        std::free(buffer);
        throw std::runtime_error("Failed to allocate zip source.");
    }

    if (zip_file_add(archive, entryPath.c_str(), source, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8) < 0)
    {
        zip_source_free(source);
        throw std::runtime_error("Failed to add zip entry.");
    }
}

std::filesystem::path CreateZipFixture(const std::filesystem::path& outputPath)
{
    int errorCode = ZIP_ER_OK;
    zip_t* archive = zip_open(outputPath.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &errorCode);

    if (archive == nullptr)
    {
        throw std::runtime_error("Failed to create zip fixture.");
    }

    AddZipEntry(archive, "first.fb2", "<fb2/>");
    AddZipEntry(archive, "second.fb2", "<fb2/>");

    if (zip_close(archive) != 0)
    {
        throw std::runtime_error("Failed to finalize zip fixture.");
    }

    return outputPath;
}

std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

class CSelectiveSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest& request,
        Librova::Domain::IProgressSink&,
        std::stop_token) const override
    {
        const auto fileName = request.SourcePath.filename().string();
        if (fileName == "duplicate.fb2")
        {
            return {
                .Status = Librova::Importing::ESingleFileImportStatus::RejectedDuplicate,
                .Warnings = {"Import rejected because a strict duplicate already exists."}
            };
        }

        if (fileName == "broken.fb2")
        {
            return {
                .Status = Librova::Importing::ESingleFileImportStatus::Failed,
                .Error = "Parser exploded."
            };
        }

        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{11}
        };
    }
};

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
    REQUIRE_FALSE(importer.LastRequest->ForceEpubConversion);
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

TEST_CASE("Library import facade forwards forced EPUB conversion to single-file imports", "[application][import]")
{
    CStubSingleFileImporter importer;
    importer.Result = {
        .Status = Librova::Importing::ESingleFileImportStatus::Imported,
        .ImportedBookId = Librova::Domain::SBookId{7}
    };
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;
    const auto sandbox = CreateImportSandbox("single-file-force-convert");

    const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    const auto result = facade.Run({
        .SourcePaths = {sandbox.SourcePath},
        .WorkingDirectory = sandbox.WorkingDirectory,
        .ForceEpubConversion = true
    }, progressSink, {});

    REQUIRE(result.IsSuccess());
    REQUIRE(importer.LastRequest.has_value());
    REQUIRE(importer.LastRequest->ForceEpubConversion);
    std::filesystem::remove_all(sandbox.Root);
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

TEST_CASE("Library import facade keeps batch mode for empty selected directory", "[application][import]")
{
    CStubSingleFileImporter importer;
    importer.Result = {
        .Status = Librova::Importing::ESingleFileImportStatus::Imported,
        .ImportedBookId = Librova::Domain::SBookId{7}
    };
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;

    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-empty-directory";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "nested");
    std::ofstream(sandbox / "nested" / "note.txt").put('x');

    const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    const auto result = facade.Run({
        .SourcePaths = {sandbox},
        .WorkingDirectory = sandbox / "work"
    }, progressSink, {});

    REQUIRE(result.Summary.Mode == Librova::Application::EImportMode::Batch);
    REQUIRE(result.Summary.TotalEntries == 0);
    REQUIRE(result.Summary.ImportedEntries == 0);
    REQUIRE(result.Summary.Warnings.size() == 1);
    REQUIRE(result.Summary.Warnings[0].find("does not contain supported") != std::string::npos);

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade keeps outer batch totals while ZIP entries are running", "[application][import]")
{
    CStubSingleFileImporter importer;
    importer.Result = {
        .Status = Librova::Importing::ESingleFileImportStatus::Imported,
        .ImportedBookId = Librova::Domain::SBookId{7}
    };
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CStructuredProgressSink progressSink;

    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-batch-zip-progress";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);
    std::ofstream(sandbox / "standalone.fb2").put('a');
    const auto zipPath = CreateZipFixture(sandbox / "batch.zip");

    const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    const auto result = facade.Run({
        .SourcePaths = {sandbox / "standalone.fb2", zipPath},
        .WorkingDirectory = sandbox / "work"
    }, progressSink, {});

    REQUIRE(result.Summary.TotalEntries == 3);
    REQUIRE_FALSE(progressSink.Snapshots.empty());
    REQUIRE(std::ranges::all_of(progressSink.Snapshots, [](const auto& snapshot) {
        return snapshot.TotalEntries == 3;
    }));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade continues batch import after unreadable ZIP source", "[application][import]")
{
    CStubSingleFileImporter importer;
    importer.Result = {
        .Status = Librova::Importing::ESingleFileImportStatus::Imported,
        .ImportedBookId = Librova::Domain::SBookId{7}
    };
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;

    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-bad-zip";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);
    std::ofstream(sandbox / "broken.zip") << "not a zip";
    std::ofstream(sandbox / "good.fb2").put('a');

    const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    const auto result = facade.Run({
        .SourcePaths = {sandbox / "broken.zip", sandbox / "good.fb2"},
        .WorkingDirectory = sandbox / "work"
    }, progressSink, {});

    REQUIRE(result.Summary.Mode == Librova::Application::EImportMode::Batch);
    REQUIRE(result.Summary.TotalEntries == 2);
    REQUIRE(result.Summary.ImportedEntries == 1);
    REQUIRE(result.Summary.FailedEntries == 1);
    REQUIRE(std::ranges::any_of(result.Summary.Warnings, [](const auto& warning) {
        return warning.find("Failed to inspect ZIP archive") != std::string::npos
            || warning.find("ZIP archive") != std::string::npos;
    }));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade logs skipped and failed batch sources into host log", "[application][import][logging]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-logging";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);
    const auto logPath = sandbox / "Logs" / "host.log";
    std::ofstream(sandbox / "duplicate.fb2").put('a');
    std::ofstream(sandbox / "broken.fb2").put('b');

    Librova::Logging::CLogging::InitializeHostLogger(logPath);

    {
        CSelectiveSingleFileImporter importer;
        Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
        CTestProgressSink progressSink;

        const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
        const auto result = facade.Run({
            .SourcePaths = {sandbox / "duplicate.fb2", sandbox / "broken.fb2"},
            .WorkingDirectory = sandbox / "work"
        }, progressSink, {});

        REQUIRE(result.Summary.SkippedEntries == 1);
        REQUIRE(result.Summary.FailedEntries == 1);
    }

    Librova::Logging::CLogging::Shutdown();

    REQUIRE(std::filesystem::exists(logPath));
    const auto logText = ReadTextFile(logPath);
    REQUIRE(logText.find("Import source skipped") != std::string::npos);
    REQUIRE(logText.find("duplicate.fb2") != std::string::npos);
    REQUIRE(logText.find("rejected-duplicate") != std::string::npos);
    REQUIRE(logText.find("Import source failed") != std::string::npos);
    REQUIRE(logText.find("broken.fb2") != std::string::npos);
    REQUIRE(logText.find("Parser exploded.") != std::string::npos);

    std::filesystem::remove_all(sandbox);
}
