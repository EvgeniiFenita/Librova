#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <zip.h>

#include "App/LibraryImportFacade.hpp"
#include "Domain/Book.hpp"
#include "Domain/BookRepository.hpp"
#include "App/ImportJobRunner.hpp"

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
        {
            std::lock_guard lock(m_mutex);
            Calls.push_back(request.SourcePath.filename().string());
        }

        if (request.SourcePath.filename() == "second.fb2")
        {
            return {
                .Status = Librova::Importing::ESingleFileImportStatus::Failed,
                .Error = "Second entry failed."
            };
        }

        std::lock_guard lock(m_mutex);
        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{static_cast<std::int64_t>(Calls.size())}
        };
    }

    mutable std::mutex m_mutex;
    mutable std::vector<std::string> Calls;
};

class CDuplicateOnlyZipSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest& request,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token) const override
    {
        progressSink.ReportValue(60, "Importing ZIP entry");

        if (request.SourcePath.filename() == "first.fb2")
        {
            return {
                .Status = Librova::Importing::ESingleFileImportStatus::RejectedDuplicate,
                .Warnings = {"Strict duplicate."}
            };
        }

        return {
            .Status = Librova::Importing::ESingleFileImportStatus::RejectedDuplicate,
            .Warnings = {"Probable duplicate."}
        };
    }
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

class CImporterThatCancelsAfterFirstSuccess final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest&,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token) const override
    {
        progressSink.ReportValue(40, "Importing source file");
        ++CallCount;

        if (CallCount == 1)
        {
            return {
                .Status = Librova::Importing::ESingleFileImportStatus::Imported,
                .ImportedBookId = Librova::Domain::SBookId{11}
            };
        }

        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Cancelled,
            .Error = "Cancelled by test."
        };
    }

    mutable int CallCount = 0;
};

class CDelayedSuccessfulImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest&,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token) const override
    {
        progressSink.ReportValue(40, "Importing source file");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        const std::scoped_lock lock(m_mutex);
        ++CallCount;
        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{CallCount}
        };
    }

    mutable std::mutex m_mutex;
    mutable std::int64_t CallCount = 0;
};

class CRollbackAwareBookRepository final : public Librova::Domain::IBookRepository
{
public:
    Librova::Domain::SBookId ReserveId() override
    {
        return {++m_nextId};
    }

    Librova::Domain::SBookId Add(const Librova::Domain::SBook& book) override
    {
        return ForceAdd(book);
    }

    Librova::Domain::SBookId ForceAdd(const Librova::Domain::SBook& book) override
    {
        m_books[book.Id.Value] = book;
        return book.Id;
    }

    std::optional<Librova::Domain::SBook> GetById(const Librova::Domain::SBookId id) const override
    {
        const auto iterator = m_books.find(id.Value);
        if (iterator == m_books.end())
        {
            return std::nullopt;
        }

        return iterator->second;
    }

    void Remove(const Librova::Domain::SBookId id) override
    {
        m_books.erase(id.Value);
    }

    void Compact(const std::function<void()>& /*onProgressTick*/ = nullptr) override
    {
        ++CompactCallCount;
    }

    mutable int CompactCallCount = 0;

private:
    std::int64_t m_nextId = 0;
    std::map<std::int64_t, Librova::Domain::SBook> m_books;
};

class CEmptyBookRepository final : public Librova::Domain::IBookRepository
{
public:
    [[nodiscard]] Librova::Domain::SBookId ReserveId() override
    {
        return {1};
    }

    [[nodiscard]] Librova::Domain::SBookId Add(const Librova::Domain::SBook& book) override
    {
        return book.Id;
    }

    [[nodiscard]] Librova::Domain::SBookId ForceAdd(const Librova::Domain::SBook& book) override
    {
        return book.Id;
    }

    [[nodiscard]] std::optional<Librova::Domain::SBook> GetById(const Librova::Domain::SBookId) const override
    {
        return std::nullopt;
    }

    void Remove(const Librova::Domain::SBookId) override
    {
    }
};

Librova::Domain::IBookRepository& GetEmptyBookRepository()
{
    static CEmptyBookRepository repository;
    return repository;
}

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

std::filesystem::path CreateDuplicateOnlyZipFixture(const std::filesystem::path& outputPath)
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
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
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

TEST_CASE("Import job runner maps probable duplicate rejection into structured failure", "[jobs][import]")
{
    CStubSingleFileImporter importer;
    importer.Result = {
        .Status = Librova::Importing::ESingleFileImportStatus::RejectedDuplicate,
        .Warnings = {"Probable duplicate"}
    };
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    Librova::Jobs::CImportJobRunner runner(facade);
    const auto sandbox = CreateImportSandbox("decision-required");

    const auto result = runner.Run({
        .SourcePaths = {sandbox.SourcePath},
        .WorkingDirectory = sandbox.WorkingDirectory
    }, {});

    REQUIRE(result.Snapshot.Status == Librova::Jobs::EJobStatus::Failed);
    REQUIRE(result.Error.has_value());
    REQUIRE(result.Error->Code == Librova::Domain::EDomainErrorCode::DuplicateRejected);
    REQUIRE(result.Snapshot.Message == "Import rejected because a duplicate already exists.");
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
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
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

TEST_CASE("Import job runner surfaces rollback cleanup residue in the cancelled terminal message", "[jobs][import][rollback]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-job-runner-cancelled-residue");
    std::ofstream(sandbox.GetPath() / "first.fb2").put('a');
    std::ofstream(sandbox.GetPath() / "second.fb2").put('b');
    const auto outsidePath = sandbox.GetPath().parent_path() / "librova-job-runner-cancelled-residue.epub";
    std::ofstream(outsidePath).put('x');

    CImporterThatCancelsAfterFirstSuccess importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CRollbackAwareBookRepository repository;
    repository.ForceAdd({
        .Id = {11},
        .Metadata = {.TitleUtf8 = "Rollback Residue"},
        .File = {
            .Format = Librova::Domain::EBookFormat::Epub,
            .ManagedPath = outsidePath
        }
    });
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator, repository, {.LibraryRoot = sandbox.GetPath()});
    Librova::Jobs::CImportJobRunner runner(facade);

    const auto result = runner.Run({
        .SourcePaths = {sandbox.GetPath() / "first.fb2", sandbox.GetPath() / "second.fb2"},
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, {});

    REQUIRE(result.Snapshot.Status == Librova::Jobs::EJobStatus::Cancelled);
    REQUIRE(result.Error.has_value());
    REQUIRE(result.Error->Code == Librova::Domain::EDomainErrorCode::Cancellation);
    REQUIRE(result.Snapshot.Message == "Import was cancelled. Some managed files or directories could not be removed during rollback.");
    REQUIRE(result.ImportResult.has_value());
    REQUIRE(result.ImportResult->HasRollbackCleanupResidue);
    REQUIRE(std::ranges::any_of(result.ImportResult->Summary.Warnings, [](const auto& warning) {
        return warning.find("left managed artifact") != std::string::npos;
    }));

    std::filesystem::remove(outsidePath);
}

TEST_CASE("Import job runner calls Compact on repository after a clean full rollback", "[jobs][import][rollback]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-job-runner-compact-after-rollback");
    std::ofstream(sandbox.GetPath() / "first.fb2").put('a');
    std::ofstream(sandbox.GetPath() / "second.fb2").put('b');

    CImporterThatCancelsAfterFirstSuccess importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CRollbackAwareBookRepository repository;
    // No pre-added book with an unsafe managed path → rollback will succeed cleanly
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator, repository, {.LibraryRoot = sandbox.GetPath()});
    Librova::Jobs::CImportJobRunner runner(facade);

    const auto result = runner.Run({
        .SourcePaths = {sandbox.GetPath() / "first.fb2", sandbox.GetPath() / "second.fb2"},
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, {});

    REQUIRE(result.Snapshot.Status == Librova::Jobs::EJobStatus::Cancelled);
    REQUIRE_FALSE(result.ImportResult->HasRollbackCleanupResidue);
    REQUIRE(repository.CompactCallCount == 1);
}


TEST_CASE("Import job runner reports partial success for ZIP import with failures", "[jobs][import]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-job-runner-zip");
    const std::filesystem::path zipPath = CreateZipFixture(sandbox.GetPath() / "archive.zip");
    CZipAwareStubSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
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
    auto calls = importer.Calls;
    std::sort(calls.begin(), calls.end());
    REQUIRE(calls == std::vector<std::string>{"first.fb2", "second.fb2"});
}

TEST_CASE("Import job runner fails when ZIP import produces no imported books", "[jobs][import]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-job-runner-skipped");
    const std::filesystem::path zipPath = CreateSkippedOnlyZipFixture(sandbox.GetPath() / "archive.zip");
    CZipAwareStubSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
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

TEST_CASE("Import job runner keeps duplicate semantics when ZIP import skips every supported entry as a duplicate", "[jobs][import]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-job-runner-duplicate-only-zip");
    const std::filesystem::path zipPath = CreateDuplicateOnlyZipFixture(sandbox.GetPath() / "archive.zip");
    CDuplicateOnlyZipSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
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
    REQUIRE(result.Error.has_value());
    REQUIRE(result.Error->Code == Librova::Domain::EDomainErrorCode::DuplicateRejected);
}

TEST_CASE("Import job runner ignores throwing progress callbacks", "[jobs][import]")
{
    CCallbackThrowingSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
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

TEST_CASE("Import job runner publishes structured batch progress snapshots", "[jobs][import]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-job-runner-structured-progress");
    const auto firstSourcePath = sandbox.GetPath() / "first.fb2";
    const auto secondSourcePath = sandbox.GetPath() / "second.fb2";
    std::ofstream(firstSourcePath).put('a');
    std::ofstream(secondSourcePath).put('b');

    CStubSingleFileImporter importer;
    importer.Result = {
        .Status = Librova::Importing::ESingleFileImportStatus::Imported,
        .ImportedBookId = Librova::Domain::SBookId{11}
    };
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    Librova::Jobs::CImportJobRunner runner(facade);
    std::vector<Librova::Jobs::SJobProgressSnapshot> snapshots;
    std::mutex snapshotsMutex;

    const auto result = runner.Run({
        .SourcePaths = {firstSourcePath, secondSourcePath},
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, {}, [&snapshots, &snapshotsMutex](const Librova::Jobs::SJobProgressSnapshot& snapshot) {
        const std::scoped_lock lock(snapshotsMutex);
        snapshots.push_back(snapshot);
    });

    REQUIRE(result.Snapshot.Status == Librova::Jobs::EJobStatus::Completed);
    REQUIRE(result.ImportResult.has_value());
    REQUIRE(result.ImportResult->Summary.TotalEntries == 2);
    REQUIRE(result.ImportResult->Summary.ImportedEntries == 2);
    REQUIRE_FALSE(snapshots.empty());

    const auto prepared = std::find_if(snapshots.begin(), snapshots.end(), [](const auto& snapshot) {
        return snapshot.Message == "Prepared import workload";
    });
    REQUIRE(prepared != snapshots.end());
    REQUIRE(prepared->TotalEntries == 2);
    REQUIRE(prepared->ProcessedEntries == 0);
    REQUIRE(prepared->ImportedEntries == 0);
    REQUIRE(prepared->FailedEntries == 0);
    REQUIRE(prepared->SkippedEntries == 0);

    const auto processed = std::find_if(snapshots.begin(), snapshots.end(), [](const auto& snapshot) {
        return snapshot.Message == "Processed source file";
    });
    REQUIRE(processed != snapshots.end());
    REQUIRE(processed->TotalEntries == 2);
    REQUIRE(processed->ProcessedEntries == 1);
    REQUIRE(processed->ImportedEntries == 1);
    REQUIRE(processed->Percent == 50);

    REQUIRE(result.Snapshot.TotalEntries == 2);
    REQUIRE(result.Snapshot.ProcessedEntries == 2);
    REQUIRE(result.Snapshot.ImportedEntries == 2);
    REQUIRE(result.Snapshot.FailedEntries == 0);
    REQUIRE(result.Snapshot.SkippedEntries == 0);
}

TEST_CASE("Import job runner keeps running imported counters monotonic during parallel batch progress", "[jobs][import]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-job-runner-monotonic-progress");
    std::vector<std::filesystem::path> sourcePaths;
    for (int i = 0; i < 8; ++i)
    {
        const auto path = sandbox.GetPath() / ("book-" + std::to_string(i) + ".fb2");
        std::ofstream(path).put('a');
        sourcePaths.push_back(path);
    }

    CDelayedSuccessfulImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    Librova::Jobs::CImportJobRunner runner(facade);

    std::vector<Librova::Jobs::SJobProgressSnapshot> snapshots;
    std::mutex snapshotsMutex;

    const auto result = runner.Run(
        {
            .SourcePaths = sourcePaths,
            .WorkingDirectory = sandbox.GetPath() / "work"
        },
        {},
        [&snapshots, &snapshotsMutex](const Librova::Jobs::SJobProgressSnapshot& snapshot)
        {
            const std::scoped_lock lock(snapshotsMutex);
            snapshots.push_back(snapshot);
        });

    REQUIRE(result.Snapshot.Status == Librova::Jobs::EJobStatus::Completed);
    REQUIRE(result.ImportResult.has_value());
    REQUIRE(result.ImportResult->Summary.ImportedEntries == sourcePaths.size());

    std::size_t lastImported = 0;
    for (const auto& snapshot : snapshots)
    {
        if (snapshot.Status != Librova::Jobs::EJobStatus::Running)
        {
            continue;
        }

        REQUIRE(snapshot.ImportedEntries >= lastImported);
        lastImported = snapshot.ImportedEntries;
    }
}

TEST_CASE("Import job runner publishes Cancelling and RollingBack status transitions during rollback",
    "[jobs][import][rollback]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-job-runner-status-transitions");
    std::ofstream(sandbox.GetPath() / "first.fb2").put('a');
    std::ofstream(sandbox.GetPath() / "second.fb2").put('b');

    CImporterThatCancelsAfterFirstSuccess importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CRollbackAwareBookRepository repository;
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator, repository,
        {.LibraryRoot = sandbox.GetPath()});
    Librova::Jobs::CImportJobRunner runner(facade);

    std::vector<Librova::Jobs::EJobStatus> observedStatuses;
    std::mutex observedStatusesMutex;
    const auto result = runner.Run(
        {
            .SourcePaths = {sandbox.GetPath() / "first.fb2", sandbox.GetPath() / "second.fb2"},
            .WorkingDirectory = sandbox.GetPath() / "work"
        },
        {},
        [&observedStatuses, &observedStatusesMutex](const Librova::Jobs::SJobProgressSnapshot& snapshot)
        {
            const std::scoped_lock lock(observedStatusesMutex);
            observedStatuses.push_back(snapshot.Status);
        });

    REQUIRE(result.Snapshot.Status == Librova::Jobs::EJobStatus::Cancelled);

    const auto hasCancelling = std::find(
        observedStatuses.begin(), observedStatuses.end(), Librova::Jobs::EJobStatus::Cancelling)
        != observedStatuses.end();
    const auto hasRollingBack = std::find(
        observedStatuses.begin(), observedStatuses.end(), Librova::Jobs::EJobStatus::RollingBack)
        != observedStatuses.end();

    REQUIRE(hasCancelling);
    REQUIRE(hasRollingBack);

    // Cancelling must appear before RollingBack in the sequence.
    const auto cancellingPos = std::find(
        observedStatuses.begin(), observedStatuses.end(), Librova::Jobs::EJobStatus::Cancelling);
    const auto rollingBackPos = std::find(
        observedStatuses.begin(), observedStatuses.end(), Librova::Jobs::EJobStatus::RollingBack);
    REQUIRE(cancellingPos < rollingBackPos);
}

