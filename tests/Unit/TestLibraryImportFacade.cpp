#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include <zip.h>

#include "Application/LibraryImportFacade.hpp"
#include "Domain/Book.hpp"
#include "Domain/BookRepository.hpp"
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

class CWorkspaceWritingSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest& request,
        Librova::Domain::IProgressSink&,
        std::stop_token) const override
    {
        Calls.push_back(request);
        std::filesystem::create_directories(request.WorkingDirectory / "covers");
        std::ofstream(request.WorkingDirectory / "covers" / "cover.jpg").put('c');

        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{static_cast<std::int64_t>(Calls.size())}
        };
    }

    mutable std::vector<Librova::Importing::SSingleFileImportRequest> Calls;
};

class CDuplicateOnlySingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest& request,
        Librova::Domain::IProgressSink&,
        std::stop_token) const override
    {
        const auto fileName = request.SourcePath.filename().string();
        if (fileName == "strict.fb2")
        {
            return {
                .Status = Librova::Importing::ESingleFileImportStatus::RejectedDuplicate,
                .Warnings = {"Strict duplicate."}
            };
        }

        return {
            .Status = Librova::Importing::ESingleFileImportStatus::DecisionRequired,
            .Warnings = {"Probable duplicate."}
        };
    }
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
        if (ThrowOnGetById.has_value() && ThrowOnGetById->Value == id.Value)
        {
            throw std::runtime_error("Injected repository lookup failure.");
        }

        const auto iterator = m_books.find(id.Value);
        if (iterator == m_books.end())
        {
            return std::nullopt;
        }

        return iterator->second;
    }

    void Remove(const Librova::Domain::SBookId id) override
    {
        RemovedIds.push_back(id);
        if (ThrowOnRemoveId.has_value() && ThrowOnRemoveId->Value == id.Value)
        {
            throw std::runtime_error("Injected repository removal failure.");
        }

        m_books.erase(id.Value);
    }

    std::optional<Librova::Domain::SBookId> ThrowOnRemoveId;
    std::optional<Librova::Domain::SBookId> ThrowOnGetById;
    std::vector<Librova::Domain::SBookId> RemovedIds;

private:
    std::int64_t m_nextId = 0;
    std::map<std::int64_t, Librova::Domain::SBook> m_books;
};

class CImporterThatCancelsAfterFirstSuccess final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest&,
        Librova::Domain::IProgressSink&,
        std::stop_token) const override
    {
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

TEST_CASE("Library import facade cleans per-source working directories after batch import", "[application][import]")
{
    CWorkspaceWritingSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-batch-workspace-cleanup";
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
    REQUIRE(result.Summary.ImportedEntries == 2);
    REQUIRE(importer.Calls.size() == 2);
    REQUIRE(importer.Calls[0].WorkingDirectory == sandbox / "work" / "entries" / "1");
    REQUIRE(importer.Calls[1].WorkingDirectory == sandbox / "work" / "entries" / "2");
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "work" / "entries" / "1" / "covers"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "work" / "entries" / "1"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "work" / "entries" / "2" / "covers"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "work" / "entries"));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade removes empty generated temp working directory after batch import", "[application][import]")
{
    CWorkspaceWritingSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-generated-temp-cleanup";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library" / "Temp");
    std::ofstream(sandbox / "one.fb2").put('a');
    std::ofstream(sandbox / "two.epub").put('b');
    const auto workingDirectory = sandbox / "Library" / "Temp" / "UiImport";

    const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator, nullptr, sandbox / "Library");
    const auto result = facade.Run({
        .SourcePaths = {sandbox / "one.fb2", sandbox / "two.epub"},
        .WorkingDirectory = workingDirectory
    }, progressSink, {});

    REQUIRE(result.Summary.Mode == Librova::Application::EImportMode::Batch);
    REQUIRE(result.Summary.ImportedEntries == 2);
    REQUIRE_FALSE(std::filesystem::exists(workingDirectory));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade preserves custom working directory outside library temp root", "[application][import]")
{
    CWorkspaceWritingSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-custom-work-preserved";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library");
    std::ofstream(sandbox / "one.fb2").put('a');
    std::ofstream(sandbox / "two.epub").put('b');
    const auto workingDirectory = sandbox / "CustomWork";

    const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator, nullptr, sandbox / "Library");
    const auto result = facade.Run({
        .SourcePaths = {sandbox / "one.fb2", sandbox / "two.epub"},
        .WorkingDirectory = workingDirectory
    }, progressSink, {});

    REQUIRE(result.Summary.Mode == Librova::Application::EImportMode::Batch);
    REQUIRE(result.Summary.ImportedEntries == 2);
    REQUIRE(std::filesystem::exists(workingDirectory));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade preserves custom working directory inside library temp root", "[application][import]")
{
    CWorkspaceWritingSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-custom-temp-work-preserved";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library" / "Temp");
    std::ofstream(sandbox / "one.fb2").put('a');
    std::ofstream(sandbox / "two.epub").put('b');
    const auto workingDirectory = sandbox / "Library" / "Temp" / "MyCustomWork";

    const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator, nullptr, sandbox / "Library");
    const auto result = facade.Run({
        .SourcePaths = {sandbox / "one.fb2", sandbox / "two.epub"},
        .WorkingDirectory = workingDirectory
    }, progressSink, {});

    REQUIRE(result.Summary.Mode == Librova::Application::EImportMode::Batch);
    REQUIRE(result.Summary.ImportedEntries == 2);
    REQUIRE(std::filesystem::exists(workingDirectory));

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

TEST_CASE("Library import facade preserves duplicate-only batch semantics when every source is skipped as a duplicate", "[application][import]")
{
    CDuplicateOnlySingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;

    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-duplicate-only-batch";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);
    std::ofstream(sandbox / "strict.fb2").put('a');
    std::ofstream(sandbox / "probable.fb2").put('b');

    const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    const auto result = facade.Run({
        .SourcePaths = {sandbox / "strict.fb2", sandbox / "probable.fb2"},
        .WorkingDirectory = sandbox / "work"
    }, progressSink, {});

    REQUIRE(result.Summary.Mode == Librova::Application::EImportMode::Batch);
    REQUIRE(result.Summary.ImportedEntries == 0);
    REQUIRE(result.Summary.FailedEntries == 0);
    REQUIRE(result.Summary.SkippedEntries == 2);
    REQUIRE(result.NoSuccessfulImportReason == Librova::Application::ENoSuccessfulImportReason::DuplicateDecisionRequired);

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade keeps a cancelled import book intact when rollback repository removal fails", "[application][import][rollback]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-rollback-failure";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Books" / "0000000011");
    std::filesystem::create_directories(sandbox / "Covers" / "0000000011");
    std::ofstream(sandbox / "first.fb2").put('a');
    std::ofstream(sandbox / "second.fb2").put('b');
    std::ofstream(sandbox / "Books" / "0000000011" / "book.epub").put('x');
    std::ofstream(sandbox / "Covers" / "0000000011" / "cover.jpg").put('y');

    CImporterThatCancelsAfterFirstSuccess importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CRollbackAwareBookRepository repository;
    repository.ForceAdd({
        .Id = {11},
        .Metadata = {.TitleUtf8 = "Rollback Book"},
        .File = {
            .Format = Librova::Domain::EBookFormat::Epub,
            .ManagedPath = std::filesystem::path{"Books"} / "0000000011" / "book.epub"
        },
        .CoverPath = std::filesystem::path{"Covers"} / "0000000011" / "cover.jpg"
    });
    repository.ThrowOnRemoveId = Librova::Domain::SBookId{11};
    CTestProgressSink progressSink;

    const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator, &repository, sandbox);
    const auto result = facade.Run({
        .SourcePaths = {sandbox / "first.fb2", sandbox / "second.fb2"},
        .WorkingDirectory = sandbox / "work"
    }, progressSink, {});

    REQUIRE(result.WasCancelled);
    REQUIRE(result.ImportedBookIds.size() == 1);
    REQUIRE(result.ImportedBookIds.front().Value == 11);
    REQUIRE(result.Summary.ImportedEntries == 1);
    REQUIRE(std::ranges::any_of(result.Summary.Warnings, [](const auto& warning) {
        return warning.find("catalog removal failed") != std::string::npos;
    }));
    REQUIRE(repository.GetById({11}).has_value());
    REQUIRE(std::filesystem::exists(sandbox / "Books" / "0000000011" / "book.epub"));
    REQUIRE(std::filesystem::exists(sandbox / "Covers" / "0000000011" / "cover.jpg"));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade keeps a cancelled import book intact when rollback repository lookup fails", "[application][import][rollback]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-rollback-lookup-failure";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Books" / "0000000011");
    std::filesystem::create_directories(sandbox / "Covers" / "0000000011");
    std::ofstream(sandbox / "first.fb2").put('a');
    std::ofstream(sandbox / "second.fb2").put('b');
    std::ofstream(sandbox / "Books" / "0000000011" / "book.epub").put('x');
    std::ofstream(sandbox / "Covers" / "0000000011" / "cover.jpg").put('y');

    CImporterThatCancelsAfterFirstSuccess importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CRollbackAwareBookRepository repository;
    repository.ForceAdd({
        .Id = {11},
        .Metadata = {.TitleUtf8 = "Rollback Lookup Book"},
        .File = {
            .Format = Librova::Domain::EBookFormat::Epub,
            .ManagedPath = std::filesystem::path{"Books"} / "0000000011" / "book.epub"
        },
        .CoverPath = std::filesystem::path{"Covers"} / "0000000011" / "cover.jpg"
    });
    repository.ThrowOnGetById = Librova::Domain::SBookId{11};
    CTestProgressSink progressSink;

    const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator, &repository, sandbox);
    const auto result = facade.Run({
        .SourcePaths = {sandbox / "first.fb2", sandbox / "second.fb2"},
        .WorkingDirectory = sandbox / "work"
    }, progressSink, {});

    REQUIRE(result.WasCancelled);
    REQUIRE(result.ImportedBookIds.size() == 1);
    REQUIRE(result.ImportedBookIds.front().Value == 11);
    REQUIRE(result.Summary.ImportedEntries == 1);
    REQUIRE(std::ranges::any_of(result.Summary.Warnings, [](const auto& warning) {
        return warning.find("catalog lookup failed") != std::string::npos;
    }));
    repository.ThrowOnGetById.reset();
    REQUIRE(repository.GetById({11}).has_value());
    REQUIRE(std::filesystem::exists(sandbox / "Books" / "0000000011" / "book.epub"));
    REQUIRE(std::filesystem::exists(sandbox / "Covers" / "0000000011" / "cover.jpg"));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade logs rollback cleanup issues for unsafe managed paths", "[application][import][logging]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-rollback-cleanup-log";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Logs");
    std::ofstream(sandbox / "first.fb2").put('a');
    std::ofstream(sandbox / "second.fb2").put('b');
    const auto outsidePath = sandbox.parent_path() / "librova-import-facade-rollback-cleanup-outside.epub";
    std::ofstream(outsidePath).put('z');
    const auto logPath = sandbox / "Logs" / "host.log";

    Librova::Logging::CLogging::InitializeHostLogger(logPath);

    {
        CImporterThatCancelsAfterFirstSuccess importer;
        Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
        CRollbackAwareBookRepository repository;
        repository.ForceAdd({
            .Id = {11},
            .Metadata = {.TitleUtf8 = "Unsafe Rollback Book"},
            .File = {
                .Format = Librova::Domain::EBookFormat::Epub,
                .ManagedPath = outsidePath
            }
        });
        CTestProgressSink progressSink;

        const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator, &repository, sandbox);
        const auto result = facade.Run({
            .SourcePaths = {sandbox / "first.fb2", sandbox / "second.fb2"},
            .WorkingDirectory = sandbox / "work"
        }, progressSink, {});

        REQUIRE(result.WasCancelled);
        REQUIRE(result.HasRollbackCleanupResidue);
    }

    Librova::Logging::CLogging::Shutdown();

    REQUIRE(std::filesystem::exists(logPath));
    const auto logText = ReadTextFile(logPath);
    REQUIRE(logText.find("rollback could not clean up managed path") != std::string::npos);
    REQUIRE(logText.find("outside.epub") != std::string::npos);

    std::filesystem::remove_all(sandbox);
    std::filesystem::remove(outsidePath);
}
