#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <zip.h>

#include "Application/LibraryImportFacade.hpp"
#include "Jobs/ImportJobRunner.hpp"

namespace {

class CStubSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest&,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token) const override
    {
        progressSink.ReportValue(40, "Importing single file");
        return Result;
    }

    Librova::Importing::SSingleFileImportResult Result;
};

class CScopedDirectory final
{
public:
    explicit CScopedDirectory(std::filesystem::path path)
        : m_path(std::move(path))
    {
        std::filesystem::remove_all(m_path);
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

class CZipAwareStubSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest& request,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token) const override
    {
        progressSink.ReportValue(60, "Importing ZIP entry");
        Calls.push_back(request.SourcePath.filename().string());

        if (request.SourcePath.filename() == "second.fb2")
        {
            return {
                .Status = Librova::Importing::ESingleFileImportStatus::Failed,
                .Error = "Second entry failed."
            };
        }

        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{static_cast<std::int64_t>(Calls.size())}
        };
    }

    mutable std::vector<std::string> Calls;
};

class CCallbackThrowingSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest&,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token) const override
    {
        progressSink.ReportValue(20, "Before throwing callback");
        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{3}
        };
    }
};

struct SImportSandbox
{
    std::filesystem::path Root;
    std::filesystem::path SourcePath;
    std::filesystem::path WorkingDirectory;
};

SImportSandbox CreateImportSandbox(const std::string_view scenario, const std::string_view fileName = "book.fb2")
{
    const auto root = std::filesystem::temp_directory_path() / ("librova-job-runner-" + std::string{scenario});
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
    AddZipEntry(archive, "notes.txt", "ignored");

    if (zip_close(archive) != 0)
    {
        throw std::runtime_error("Failed to finalize zip fixture.");
    }

    return outputPath;
}

std::filesystem::path CreateSkippedOnlyZipFixture(const std::filesystem::path& outputPath)
{
    int errorCode = ZIP_ER_OK;
    zip_t* archive = zip_open(outputPath.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &errorCode);

    if (archive == nullptr)
    {
        throw std::runtime_error("Failed to create zip fixture.");
    }

    AddZipEntry(archive, "notes.txt", "ignored");
    AddZipEntry(archive, "nested/archive.zip", "nested");

    if (zip_close(archive) != 0)
    {
        throw std::runtime_error("Failed to finalize zip fixture.");
    }

    return outputPath;
}

} // namespace

TEST_CASE("Import job runner reports completed status for successful import", "[jobs][import]")
{
    CStubSingleFileImporter importer;
    importer.Result = {
        .Status = Librova::Importing::ESingleFileImportStatus::Imported,
        .ImportedBookId = Librova::Domain::SBookId{5}
    };
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Jobs::CImportJobRunner runner(facade);
    const auto sandbox = CreateImportSandbox("success");

    const auto result = runner.Run({
        .SourcePaths = {sandbox.SourcePath},
        .WorkingDirectory = sandbox.WorkingDirectory
    }, {});

    REQUIRE(result.Snapshot.Status == Librova::Jobs::EJobStatus::Completed);
    REQUIRE(result.Snapshot.Percent == 100);
    REQUIRE(result.Snapshot.Message == "Import completed successfully.");
    REQUIRE_FALSE(result.Error.has_value());
    REQUIRE(result.ImportResult.has_value());
    std::filesystem::remove_all(sandbox.Root);
}

TEST_CASE("Import job runner maps duplicate decision required into structured failure", "[jobs][import]")
{
    CStubSingleFileImporter importer;
    importer.Result = {
        .Status = Librova::Importing::ESingleFileImportStatus::DecisionRequired,
        .Warnings = {"Probable duplicate"}
    };
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Jobs::CImportJobRunner runner(facade);
    const auto sandbox = CreateImportSandbox("decision-required");

    const auto result = runner.Run({
        .SourcePaths = {sandbox.SourcePath},
        .WorkingDirectory = sandbox.WorkingDirectory
    }, {});

    REQUIRE(result.Snapshot.Status == Librova::Jobs::EJobStatus::Failed);
    REQUIRE(result.Error.has_value());
    REQUIRE(result.Error->Code == Librova::Domain::EDomainErrorCode::DuplicateDecisionRequired);
    REQUIRE(result.Snapshot.Message == "Import requires user confirmation for a probable duplicate.");
    std::filesystem::remove_all(sandbox.Root);
}

TEST_CASE("Import job runner maps cancellation into cancelled job state", "[jobs][import]")
{
    CStubSingleFileImporter importer;
    importer.Result = {
        .Status = Librova::Importing::ESingleFileImportStatus::Cancelled,
        .Warnings = {"Conversion cancelled."}
    };
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Jobs::CImportJobRunner runner(facade);
    const auto sandbox = CreateImportSandbox("cancelled");

    const auto result = runner.Run({
        .SourcePaths = {sandbox.SourcePath},
        .WorkingDirectory = sandbox.WorkingDirectory
    }, {});

    REQUIRE(result.Snapshot.Status == Librova::Jobs::EJobStatus::Cancelled);
    REQUIRE(result.Error.has_value());
    REQUIRE(result.Error->Code == Librova::Domain::EDomainErrorCode::Cancellation);
    REQUIRE(result.Snapshot.Message == "Import was cancelled.");
    std::filesystem::remove_all(sandbox.Root);
}

TEST_CASE("Import job runner reports partial success for ZIP import with failures", "[jobs][import]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-job-runner-zip");
    const std::filesystem::path zipPath = CreateZipFixture(sandbox.GetPath() / "archive.zip");
    CZipAwareStubSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Jobs::CImportJobRunner runner(facade);

    const auto result = runner.Run({
        .SourcePaths = {zipPath},
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, {});

    REQUIRE(result.ImportResult.has_value());
    REQUIRE(result.ImportResult->Summary.Mode == Librova::Application::EImportMode::ZipArchive);
    REQUIRE(result.ImportResult->Summary.TotalEntries == 3);
    REQUIRE(result.ImportResult->Summary.ImportedEntries == 1);
    REQUIRE(result.ImportResult->Summary.FailedEntries == 1);
    REQUIRE(result.ImportResult->Summary.SkippedEntries == 1);
    REQUIRE(result.Snapshot.Status == Librova::Jobs::EJobStatus::Completed);
    REQUIRE(result.Snapshot.Message == "Import completed with partial success.");
    REQUIRE_FALSE(result.Error.has_value());
    REQUIRE(importer.Calls == std::vector<std::string>{"first.fb2", "second.fb2"});
}

TEST_CASE("Import job runner fails when ZIP import produces no imported books", "[jobs][import]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-job-runner-skipped");
    const std::filesystem::path zipPath = CreateSkippedOnlyZipFixture(sandbox.GetPath() / "archive.zip");
    CZipAwareStubSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Jobs::CImportJobRunner runner(facade);

    const auto result = runner.Run({
        .SourcePaths = {zipPath},
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, {});

    REQUIRE(result.ImportResult.has_value());
    REQUIRE(result.ImportResult->Summary.ImportedEntries == 0);
    REQUIRE(result.ImportResult->Summary.FailedEntries == 0);
    REQUIRE(result.ImportResult->Summary.SkippedEntries == 2);
    REQUIRE(result.Snapshot.Status == Librova::Jobs::EJobStatus::Failed);
    REQUIRE(result.Snapshot.Message == "Import completed without importing any supported books.");
    REQUIRE(result.Error.has_value());
    REQUIRE(result.Error->Code == Librova::Domain::EDomainErrorCode::UnsupportedFormat);
}

TEST_CASE("Import job runner ignores throwing progress callbacks", "[jobs][import]")
{
    CCallbackThrowingSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Jobs::CImportJobRunner runner(facade);
    const auto sandbox = CreateImportSandbox("observer-failure");

    const auto result = runner.Run({
        .SourcePaths = {sandbox.SourcePath},
        .WorkingDirectory = sandbox.WorkingDirectory
    }, {}, [](const Librova::Jobs::SJobProgressSnapshot&) {
        throw std::runtime_error("observer failure");
    });

    REQUIRE(result.Snapshot.Status == Librova::Jobs::EJobStatus::Completed);
    REQUIRE_FALSE(result.Error.has_value());
    REQUIRE(result.ImportResult.has_value());
    std::filesystem::remove_all(sandbox.Root);
}
