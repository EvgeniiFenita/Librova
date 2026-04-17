#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include <zip.h>

#include "Application/LibraryImportFacade.hpp"
#include "Application/StructuredProgressMapper.hpp"
#include "Domain/Book.hpp"
#include "Domain/BookRepository.hpp"
#include "Logging/Logging.hpp"
#include "TestStructuredProgressSink.hpp"

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
        const std::scoped_lock lock(m_mutex);
        LastRequest = request;
        return Result;
    }

    mutable std::mutex m_mutex;
    mutable std::optional<Librova::Importing::SSingleFileImportRequest> LastRequest;
    Librova::Importing::SSingleFileImportResult Result;
};

class COrderedSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest& request,
        Librova::Domain::IProgressSink&,
        std::stop_token) const override
    {
        const std::scoped_lock lock(m_mutex);
        Calls.push_back(request.SourcePath.filename().string());
        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{static_cast<std::int64_t>(Calls.size())}
        };
    }

    mutable std::mutex m_mutex;
    mutable std::vector<std::string> Calls;
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

std::size_t CountOccurrences(const std::string& haystack, const std::string_view needle)
{
    std::size_t count = 0;
    std::size_t offset = 0;
    while ((offset = haystack.find(needle, offset)) != std::string::npos)
    {
        ++count;
        offset += needle.size();
    }
    return count;
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
        std::filesystem::create_directories(request.WorkingDirectory / "covers");
        std::ofstream(request.WorkingDirectory / "covers" / "cover.jpg").put('c');

        std::lock_guard lock(m_mutex);
        Calls.push_back(request);
        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{static_cast<std::int64_t>(Calls.size())}
        };
    }

    mutable std::mutex m_mutex;
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
            .Status = Librova::Importing::ESingleFileImportStatus::RejectedDuplicate,
            .Warnings = {"Probable duplicate."}
        };
    }
};

class CSlowFirstSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest& request,
        Librova::Domain::IProgressSink&,
        std::stop_token) const override
    {
        const auto fileName = request.SourcePath.filename().string();
        if (fileName == "slow.fb2")
        {
            std::unique_lock lock(m_mutex);
            m_cv.wait(lock, [this] { return m_releaseSlow; });
        }
        else
        {
            const std::scoped_lock lock(m_mutex);
            ++m_fastCompleted;
            m_cv.notify_all();
        }

        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{++m_nextId}
        };
    }

    void WaitForFastCompletions(const std::size_t expectedCount) const
    {
        std::unique_lock lock(m_mutex);
        m_cv.wait(lock, [&] { return m_fastCompleted >= expectedCount; });
    }

    void ReleaseSlow()
    {
        const std::scoped_lock lock(m_mutex);
        m_releaseSlow = true;
        m_cv.notify_all();
    }

private:
    mutable std::mutex m_mutex;
    mutable std::condition_variable m_cv;
    mutable std::size_t m_fastCompleted = 0;
    mutable bool m_releaseSlow = false;
    mutable std::int64_t m_nextId = 0;
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

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox.Root});
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

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox.Root});
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

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox});
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

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox});
    const auto result = facade.Run({
        .SourcePaths = {sandbox / "one.fb2", sandbox / "two.epub"},
        .WorkingDirectory = sandbox / "work"
    }, progressSink, {});

    REQUIRE(result.Summary.Mode == Librova::Application::EImportMode::Batch);
    REQUIRE(result.Summary.ImportedEntries == 2);
    REQUIRE(importer.Calls.size() == 2);

    std::vector<std::filesystem::path> actualWorkDirs;
    for (const auto& call : importer.Calls)
        actualWorkDirs.push_back(call.WorkingDirectory);
    std::sort(actualWorkDirs.begin(), actualWorkDirs.end());
    REQUIRE(actualWorkDirs[0] == sandbox / "work" / "entries" / "1");
    REQUIRE(actualWorkDirs[1] == sandbox / "work" / "entries" / "2");

    REQUIRE_FALSE(std::filesystem::exists(sandbox / "work" / "entries" / "1" / "covers"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "work" / "entries" / "1"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "work" / "entries" / "2" / "covers"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "work" / "entries"));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade removes empty generated runtime working directory after batch import", "[application][import]")
{
    CWorkspaceWritingSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-generated-runtime-cleanup";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library");
    std::ofstream(sandbox / "one.fb2").put('a');
    std::ofstream(sandbox / "two.epub").put('b');
    const auto workingDirectory = sandbox / "Runtime" / "ABC12345" / "ImportWorkspaces" / "GeneratedUiImportWorkspace";

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox / "Library"});
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

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox / "Library"});
    const auto result = facade.Run({
        .SourcePaths = {sandbox / "one.fb2", sandbox / "two.epub"},
        .WorkingDirectory = workingDirectory
    }, progressSink, {});

    REQUIRE(result.Summary.Mode == Librova::Application::EImportMode::Batch);
    REQUIRE(result.Summary.ImportedEntries == 2);
    REQUIRE(std::filesystem::exists(workingDirectory));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade preserves custom working directory that only reuses generated leaf name", "[application][import]")
{
    CWorkspaceWritingSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-generated-leaf-preserved";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library");
    std::ofstream(sandbox / "one.fb2").put('a');
    std::ofstream(sandbox / "two.epub").put('b');
    const auto workingDirectory = sandbox / "CustomWork" / "GeneratedUiImportWorkspace";

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox / "Library"});
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

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox / "Library"});
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

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox.Root});
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

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox});
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

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox});
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

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox});
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

TEST_CASE("Library import facade accumulates imported counter across sources without reset", "[application][import]")
{
    // Regression test: before the fix, CScopedStructuredProgressSink forwarded
    // the per-source-local importedEntries/failedEntries/skippedEntries to the
    // outer sink without adding the already-accumulated prior values.  When the
    // second source started it would effectively reset the imported counter.
    //
    // Two ZIPs are used intentionally: each ZIP emits per-entry structured
    // progress snapshots via CZipImportCoordinator, so the outer sink receives
    // snapshots both DURING zip1 and DURING zip2.  With the old (broken) code
    // zip2 would restart its local counter at 0, causing a visible drop in the
    // reported importedEntries (e.g. 2 → 1).  The monotonic-decrease check
    // below catches exactly that regression.  A single-file stub source does not
    // emit structured progress events, so it cannot exercise this path.
    CStubSingleFileImporter importer;
    importer.Result = {
        .Status = Librova::Importing::ESingleFileImportStatus::Imported,
        .ImportedBookId = Librova::Domain::SBookId{1}
    };
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CStructuredProgressSink progressSink;

    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-accumulated-progress";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);
    // zip1 with 2 entries, then zip2 with 2 entries — 4 entries total, all Imported.
    const auto zip1Path = CreateZipFixture(sandbox / "batch1.zip");
    const auto zip2Path = CreateZipFixture(sandbox / "batch2.zip");

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox});
    const auto result = facade.Run({
        .SourcePaths = {zip1Path, zip2Path},
        .WorkingDirectory = sandbox / "work"
    }, progressSink, {});

    REQUIRE(result.Summary.ImportedEntries == 4);
    REQUIRE_FALSE(progressSink.Snapshots.empty());

    // importedEntries must never decrease across consecutive snapshots.
    // With the old code zip2 restarted at 0, causing a drop from 2 → 1 on the
    // first entry of zip2 — this check catches that regression directly.
    std::size_t maxSeen = 0;
    for (const auto& snapshot : progressSink.Snapshots)
    {
        REQUIRE(snapshot.ImportedEntries >= maxSeen);
        maxSeen = snapshot.ImportedEntries;
    }

    // The very last snapshot must agree with the final summary.
    const auto& last = progressSink.Snapshots.back();
    REQUIRE(last.ImportedEntries == result.Summary.ImportedEntries);

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade accepts existing directories during source validation without recursive expansion", "[application][import]")
{
    CStubSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);

    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-validation-directory";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "nested");
    std::ofstream(sandbox / "nested" / "note.txt").put('x');

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox});
    const auto validation = facade.ValidateImportSources({sandbox});

    REQUIRE_FALSE(validation.BlockingMessage.has_value());

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

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox});
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

        const Librova::Application::CLibraryImportFacade facade(
            importer,
            zipCoordinator,
            GetEmptyBookRepository(),
            {.LibraryRoot = sandbox});
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

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox});
    const auto result = facade.Run({
        .SourcePaths = {sandbox / "strict.fb2", sandbox / "probable.fb2"},
        .WorkingDirectory = sandbox / "work"
    }, progressSink, {});

    REQUIRE(result.Summary.Mode == Librova::Application::EImportMode::Batch);
    REQUIRE(result.Summary.ImportedEntries == 0);
    REQUIRE(result.Summary.FailedEntries == 0);
    REQUIRE(result.Summary.SkippedEntries == 2);
    REQUIRE(result.NoSuccessfulImportReason == Librova::Application::ENoSuccessfulImportReason::DuplicateRejected);

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

    const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator, repository, {.LibraryRoot = sandbox});
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

    const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator, repository, {.LibraryRoot = sandbox});
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

        const Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator, repository, {.LibraryRoot = sandbox});
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

TEST_CASE("CScopedStructuredProgressSink accumulates prior imported entries in forwarded report", "[application][progress]")
{
    // Direct unit test for the regression: prior imported/failed/skipped entries
    // must be added in ReportStructuredProgress, not only stored in the constructor.
    CStructuredProgressSink outer;
    Librova::Application::CScopedStructuredProgressSink sink(
        outer,
        /*totalEntries=*/10,
        /*processedEntries=*/3,
        /*contributionEntries=*/2,
        /*priorImportedEntries=*/5,
        /*priorFailedEntries=*/1,
        /*priorSkippedEntries=*/2);

    sink.ReportStructuredProgress(2, 1, 1, 0, 0, 50, "mid");

    REQUIRE(outer.Snapshots.size() == 1);
    // processedEntries: m_processedEntries(3) + local(1) = 4
    REQUIRE(outer.Snapshots[0].ProcessedEntries == 4);
    // importedEntries: prior(5) + local(1) = 6
    REQUIRE(outer.Snapshots[0].ImportedEntries == 6);
    // failedEntries: prior(1) + local(0) = 1
    REQUIRE(outer.Snapshots[0].FailedEntries == 1);
    // skippedEntries: prior(2) + local(0) = 2
    REQUIRE(outer.Snapshots[0].SkippedEntries == 2);
}

TEST_CASE("CScopedStructuredProgressSink with default prior values behaves as before", "[application][progress]")
{
    // When priorImportedEntries/failedEntries/skippedEntries default to 0,
    // behaviour must be identical to the original constructor.
    CStructuredProgressSink outer;
    Librova::Application::CScopedStructuredProgressSink sink(
        outer,
        /*totalEntries=*/4,
        /*processedEntries=*/0,
        /*contributionEntries=*/2);

    sink.ReportStructuredProgress(2, 2, 2, 1, 0, 100, "done");

    REQUIRE(outer.Snapshots.size() == 1);
    REQUIRE(outer.Snapshots[0].ImportedEntries == 2);
    REQUIRE(outer.Snapshots[0].FailedEntries == 1);
    REQUIRE(outer.Snapshots[0].SkippedEntries == 0);
}

// ---------------------------------------------------------------------------
// Parallel batch tests
// ---------------------------------------------------------------------------

namespace {

class CAtomicCancellingSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest&,
        Librova::Domain::IProgressSink&,
        std::stop_token) const override
    {
        const int call = ++CallCount;
        if (call == 1)
        {
            return {
                .Status = Librova::Importing::ESingleFileImportStatus::Imported,
                .ImportedBookId = Librova::Domain::SBookId{11}
            };
        }

        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Cancelled,
            .Error  = "Cancelled by test."
        };
    }

    mutable std::atomic<int> CallCount{0};
};

// Records all SSingleFileImportRequest objects passed to Run() in a thread-safe manner.
class CSha256TrackingImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest& req,
        Librova::Domain::IProgressSink&,
        std::stop_token) const override
    {
        {
            std::lock_guard lock(m_mutex);
            CapturedSha256.push_back(req.Sha256Hex.value_or("(none)"));
        }
        return {
            .Status         = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{++m_nextId}
        };
    }

    mutable std::mutex                 m_mutex;
    mutable std::atomic<std::int64_t>  m_nextId{0};
    mutable std::vector<std::string>   CapturedSha256;
};

} // namespace

TEST_CASE("Library import facade parallel batch reports correct counts on partial failure", "[application][import]")
{
    CSelectiveSingleFileImporter importer; // "broken.fb2" -> Failed; others -> Imported
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;

    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-parallel-partial";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);
    std::ofstream(sandbox / "good1.fb2").put('a');
    std::ofstream(sandbox / "broken.fb2").put('b');
    std::ofstream(sandbox / "good2.fb2").put('c');

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox});
    const auto result = facade.Run({
        .SourcePaths = {sandbox / "good1.fb2", sandbox / "broken.fb2", sandbox / "good2.fb2"},
        .WorkingDirectory = sandbox / "work"
    }, progressSink, {});

    REQUIRE(result.Summary.Mode == Librova::Application::EImportMode::Batch);
    REQUIRE(result.Summary.TotalEntries == 3);
    REQUIRE(result.Summary.ImportedEntries == 2);
    REQUIRE(result.Summary.FailedEntries == 1);
    REQUIRE(result.Summary.SkippedEntries == 0);

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade parallel batch reports all skipped when every file is a duplicate", "[application][import]")
{
    CDuplicateOnlySingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;

    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-parallel-alldup";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);
    std::ofstream(sandbox / "dup1.fb2").put('a');
    std::ofstream(sandbox / "dup2.fb2").put('b');
    std::ofstream(sandbox / "dup3.fb2").put('c');

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox});
    const auto result = facade.Run({
        .SourcePaths = {sandbox / "dup1.fb2", sandbox / "dup2.fb2", sandbox / "dup3.fb2"},
        .WorkingDirectory = sandbox / "work"
    }, progressSink, {});

    REQUIRE(result.Summary.Mode == Librova::Application::EImportMode::Batch);
    REQUIRE(result.Summary.TotalEntries == 3);
    REQUIRE(result.Summary.ImportedEntries == 0);
    REQUIRE(result.Summary.SkippedEntries == 3);

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade parallel batch marks result as cancelled when importer returns Cancelled", "[application][import]")
{
    CAtomicCancellingSingleFileImporter importer; // first call -> Imported, rest -> Cancelled
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;

    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-parallel-cancel";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);
    std::ofstream(sandbox / "a.fb2").put('1');
    std::ofstream(sandbox / "b.fb2").put('2');
    std::ofstream(sandbox / "c.fb2").put('3');

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox});
    const auto result = facade.Run({
        .SourcePaths = {sandbox / "a.fb2", sandbox / "b.fb2", sandbox / "c.fb2"},
        .WorkingDirectory = sandbox / "work"
    }, progressSink, {});

    REQUIRE(result.Summary.Mode == Librova::Application::EImportMode::Batch);
    REQUIRE(result.WasCancelled);
    REQUIRE(result.Summary.ImportedEntries == 0);
    REQUIRE(result.Summary.FailedEntries == 0);
    REQUIRE(result.Summary.SkippedEntries == 2);

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade parallel batch rolls back imported book on cancellation", "[application][import]")
{
    // First Run() call returns Imported(bookId=11); all subsequent calls return Cancelled.
    // The facade should detect WasCancelled, trigger rollback, and end with no imported books.
    CAtomicCancellingSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;

    auto rollbackRepo = std::make_unique<CRollbackAwareBookRepository>();
    auto& rollbackRepoRef = *rollbackRepo;

    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-rollback-test";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);
    std::ofstream(sandbox / "book1.fb2").put('a');
    std::ofstream(sandbox / "book2.fb2").put('b');
    std::ofstream(sandbox / "book3.fb2").put('c');

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        *rollbackRepo,
        {.LibraryRoot = sandbox});

    const auto result = facade.Run({
        .SourcePaths = {sandbox / "book1.fb2", sandbox / "book2.fb2", sandbox / "book3.fb2"},
        .WorkingDirectory = sandbox / "work"
    }, progressSink, {});

    REQUIRE(result.WasCancelled);
    REQUIRE(result.ImportedBookIds.empty());
    REQUIRE(rollbackRepoRef.RemovedIds.size() == 1);
    REQUIRE(rollbackRepoRef.RemovedIds[0].Value == 11);

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade does not forward Sha256Hex to individual importers in parallel batch mode", "[application][import]")
{
    // In multi-file (batch) mode Sha256Hex is not known per-entry, so the facade must NOT
    // forward the top-level request.Sha256Hex to individual SSingleFileImportRequest calls.
    CSha256TrackingImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;
    CRollbackAwareBookRepository repo;

    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-sha256-batch-test";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);
    std::ofstream(sandbox / "a.fb2").put('a');
    std::ofstream(sandbox / "b.fb2").put('b');
    std::ofstream(sandbox / "c.fb2").put('c');

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        repo,
        {.LibraryRoot = sandbox});

    const auto result = facade.Run({
        .SourcePaths      = {sandbox / "a.fb2", sandbox / "b.fb2", sandbox / "c.fb2"},
        .WorkingDirectory = sandbox / "work",
        .Sha256Hex        = "deadbeef"
    }, progressSink, {});

    // All three books must have been processed.
    REQUIRE(result.Summary.TotalEntries == 3);

    // None of the per-entry import calls should have received a Sha256Hex value.
    for (const auto& captured : importer.CapturedSha256)
    {
        REQUIRE(captured == "(none)");
    }

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade preserves original source order when a standalone file appears before a ZIP source", "[application][import]")
{
    COrderedSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;

    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-mixed-order";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);
    std::ofstream(sandbox / "standalone.fb2").put('a');
    const auto zipPath = CreateZipFixture(sandbox / "batch.zip");

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox});
    const auto result = facade.Run({
        .SourcePaths = {sandbox / "standalone.fb2", zipPath},
        .WorkingDirectory = sandbox / "work"
    }, progressSink, {});

    REQUIRE(result.Summary.ImportedEntries == 3);
    REQUIRE_FALSE(importer.Calls.empty());
    REQUIRE(importer.Calls.front() == "standalone.fb2");

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade reports progress for completed files before the first batch slot finishes", "[application][import][progress]")
{
    if (std::thread::hardware_concurrency() < 2U)
    {
        SUCCEED("Parallel batch progress test requires at least two hardware threads.");
        return;
    }

    CSlowFirstSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CThreadSafeStructuredProgressSink progressSink;

    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-slow-first-progress";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);
    std::ofstream(sandbox / "slow.fb2").put('s');
    std::ofstream(sandbox / "fast1.fb2").put('a');
    std::ofstream(sandbox / "fast2.fb2").put('b');

    const Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = sandbox});

    auto futureResult = std::async(std::launch::async, [&] {
        return facade.Run({
            .SourcePaths = {sandbox / "slow.fb2", sandbox / "fast1.fb2", sandbox / "fast2.fb2"},
            .WorkingDirectory = sandbox / "work"
        }, progressSink, {});
    });

    importer.WaitForFastCompletions(2);

    REQUIRE(progressSink.WaitForPartialImportedProgress());

    const auto snapshotsBeforeSlowRelease = progressSink.SnapshotCopy();
    REQUIRE(std::ranges::any_of(snapshotsBeforeSlowRelease, [](const auto& snapshot) {
        return snapshot.ImportedEntries >= 1 && snapshot.ProcessedEntries < snapshot.TotalEntries;
    }));

    importer.ReleaseSlow();
    const auto result = futureResult.get();

    REQUIRE(result.Summary.ImportedEntries == 3);

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library import facade logs one run-level import perf summary that includes ZIP stages", "[application][import][logging]")
{
    CStubSingleFileImporter importer;
    importer.Result = {
        .Status = Librova::Importing::ESingleFileImportStatus::Imported,
        .ImportedBookId = Librova::Domain::SBookId{7}
    };
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CTestProgressSink progressSink;

    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-facade-import-perf";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);
    std::ofstream(sandbox / "standalone.fb2").put('a');
    const auto zipPath = CreateZipFixture(sandbox / "batch.zip");
    const auto logPath = sandbox / "Logs" / "host.log";

    Librova::Logging::CLogging::InitializeHostLogger(logPath);

    {
        const Librova::Application::CLibraryImportFacade facade(
            importer,
            zipCoordinator,
            GetEmptyBookRepository(),
            {.LibraryRoot = sandbox});
        const auto result = facade.Run({
            .SourcePaths = {sandbox / "standalone.fb2", zipPath},
            .WorkingDirectory = sandbox / "work"
        }, progressSink, {});

        REQUIRE(result.Summary.ImportedEntries == 3);
    }

    Librova::Logging::CLogging::Shutdown();

    const auto logText = ReadTextFile(logPath);
    REQUIRE(CountOccurrences(logText, "[import-perf] SUMMARY") == 1);
    REQUIRE(logText.find("zip_scan=") != std::string::npos);
    REQUIRE(logText.find("zip_extract=") != std::string::npos);

    std::filesystem::remove_all(sandbox);
}
