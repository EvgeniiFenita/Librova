#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <zip.h>

#include "Importing/SingleFileImportCoordinator.hpp"
#include "Logging/Logging.hpp"
#include "TestStructuredProgressSink.hpp"
#include "Unicode/UnicodeConversion.hpp"
#include "ZipImporting/ZipImportCoordinator.hpp"

namespace {

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

class CTestProgressSink final : public Librova::Domain::IProgressSink
{
public:
    void ReportValue(const int percent, std::string_view message) override
    {
        LastPercent = percent;
        LastMessage.assign(message);
    }

    bool IsCancellationRequested() const override
    {
        return false;
    }

    int LastPercent = 0;
    std::string LastMessage;
};

class CStubBookRepository final : public Librova::Domain::IBookRepository
{
public:
    [[nodiscard]] Librova::Domain::SBookId ReserveId() override
    {
        return Librova::Domain::SBookId{m_nextId.fetch_add(1, std::memory_order_relaxed)};
    }

    [[nodiscard]] Librova::Domain::SBookId Add(const Librova::Domain::SBook& book) override
    {
        return book.Id;
    }

    [[nodiscard]] Librova::Domain::SBookId ForceAdd(const Librova::Domain::SBook& book) override
    {
        return book.Id;
    }

    [[nodiscard]] std::optional<Librova::Domain::SBook> GetById(Librova::Domain::SBookId) const override
    {
        return std::nullopt;
    }

    void Remove(const Librova::Domain::SBookId) override
    {
    }

private:
    mutable std::atomic<std::int64_t> m_nextId{1};
};

class CStubSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest& request,
        Librova::Domain::IProgressSink&,
        std::stop_token) const override
    {
        std::size_t callIndex = 0;
        {
            std::lock_guard lock(m_mutex);
            Calls.push_back(request);
            callIndex = Calls.size();
        }

        if (request.SourcePath.filename() == "second.fb2")
        {
            return {
                .Status = Librova::Importing::ESingleFileImportStatus::RejectedDuplicate,
                .Warnings = {"Duplicate"}
            };
        }

        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{static_cast<std::int64_t>(callIndex)}
        };
    }

    mutable std::mutex m_mutex;
    mutable std::vector<Librova::Importing::SSingleFileImportRequest> Calls;
};

class CWorkspaceWritingSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest& request,
        Librova::Domain::IProgressSink&,
        std::stop_token) const override
    {
        std::size_t callIndex = 0;
        {
            std::lock_guard lock(m_mutex);
            Calls.push_back(request);
            callIndex = Calls.size();
        }
        std::filesystem::create_directories(request.WorkingDirectory / "covers");
        std::ofstream(request.WorkingDirectory / "covers" / "cover.jpg").put('c');

        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{static_cast<std::int64_t>(callIndex)}
        };
    }

    mutable std::mutex m_mutex;
    mutable std::vector<Librova::Importing::SSingleFileImportRequest> Calls;
};

class CEmptyWorkspaceSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest& request,
        Librova::Domain::IProgressSink&,
        std::stop_token) const override
    {
        std::size_t callIndex = 0;
        {
            std::lock_guard lock(m_mutex);
            Calls.push_back(request);
            callIndex = Calls.size();
        }
        std::filesystem::create_directories(request.WorkingDirectory / "covers");
        std::ofstream(request.WorkingDirectory / "covers" / "cover.jpg").put('c');
        std::filesystem::remove(request.WorkingDirectory / "covers" / "cover.jpg");

        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{static_cast<std::int64_t>(callIndex)}
        };
    }

    mutable std::mutex m_mutex;
    mutable std::vector<Librova::Importing::SSingleFileImportRequest> Calls;
};

class CSlowFirstZipSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest& request,
        Librova::Domain::IProgressSink&,
        std::stop_token) const override
    {
        const auto fileName = request.SourcePath.filename().string();
        if (fileName == "first.fb2")
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

    void WaitForFastCompletion() const
    {
        std::unique_lock lock(m_mutex);
        m_cv.wait(lock, [this] { return m_fastCompleted >= 1; });
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
    const auto utf8Path = Librova::Unicode::PathToUtf8(outputPath);
    zip_t* archive = zip_open(utf8Path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &errorCode);

    if (archive == nullptr)
    {
        throw std::runtime_error("Failed to create zip fixture.");
    }

    AddZipEntry(archive, "folder/first.fb2", "<fb2/>");
    AddZipEntry(archive, "second.fb2", "<fb2/>");
    AddZipEntry(archive, "notes.txt", "ignore-me");
    AddZipEntry(archive, "nested/archive.zip", "nested-not-supported");

    if (zip_close(archive) != 0)
    {
        throw std::runtime_error("Failed to finalize zip fixture.");
    }

    return outputPath;
}

std::filesystem::path CreateUnsafeZipFixture(const std::filesystem::path& outputPath)
{
    int errorCode = ZIP_ER_OK;
    const auto utf8Path = Librova::Unicode::PathToUtf8(outputPath);
    zip_t* archive = zip_open(utf8Path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &errorCode);

    if (archive == nullptr)
    {
        throw std::runtime_error("Failed to create zip fixture.");
    }

    AddZipEntry(archive, "../outside.fb2", "<fb2/>");
    AddZipEntry(archive, "safe/inside.fb2", "<fb2/>");

    if (zip_close(archive) != 0)
    {
        throw std::runtime_error("Failed to finalize zip fixture.");
    }

    return outputPath;
}

std::filesystem::path CreateNumericZipFixture(const std::filesystem::path& outputPath)
{
    int errorCode = ZIP_ER_OK;
    const auto utf8Path = Librova::Unicode::PathToUtf8(outputPath);
    zip_t* archive = zip_open(utf8Path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &errorCode);

    if (archive == nullptr)
    {
        throw std::runtime_error("Failed to create zip fixture.");
    }

    AddZipEntry(archive, "689669.fb2", "<fb2/>");
    AddZipEntry(archive, "689670.fb2", "<fb2/>");

    if (zip_close(archive) != 0)
    {
        throw std::runtime_error("Failed to finalize zip fixture.");
    }

    return outputPath;
}

std::filesystem::path CreateCyrillicZipFixture(const std::filesystem::path& outputPath)
{
    int errorCode = ZIP_ER_OK;
    const auto utf8Path = Librova::Unicode::PathToUtf8(outputPath);
    zip_t* archive = zip_open(utf8Path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &errorCode);

    if (archive == nullptr)
    {
        throw std::runtime_error("Failed to create zip fixture.");
    }

    AddZipEntry(archive, "папка/книга.fb2", "<fb2/>");

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

} // namespace

TEST_CASE("ZIP import coordinator imports supported entries and keeps partial success", "[zip-import]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-zip-import");
    const std::filesystem::path zipPath = CreateZipFixture(sandbox.GetPath() / "books.zip");
    CStubSingleFileImporter importer;
    CTestProgressSink progressSink;

    const Librova::ZipImporting::CZipImportCoordinator coordinator(importer);
    const auto result = coordinator.Run({
        .ZipPath = zipPath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .JobId = 44,
        .AllowProbableDuplicates = true
    }, progressSink, {});

    REQUIRE(importer.Calls.size() == 2);
    REQUIRE(importer.Calls[0].SourcePath.filename() == "first.fb2");
    REQUIRE(importer.Calls[1].SourcePath.filename() == "second.fb2");
    REQUIRE(importer.Calls[0].ImportJobId == 44);
    REQUIRE(importer.Calls[1].ImportJobId == 44);
    REQUIRE(importer.Calls[0].LogicalSourceLabel == std::optional<std::string>{
        Librova::Unicode::PathToUtf8(zipPath) + "::" + Librova::Unicode::PathToUtf8(std::filesystem::path{"folder/first.fb2"})});
    REQUIRE(importer.Calls[1].LogicalSourceLabel == std::optional<std::string>{
        Librova::Unicode::PathToUtf8(zipPath) + "::" + Librova::Unicode::PathToUtf8(std::filesystem::path{"second.fb2"})});
    REQUIRE(importer.Calls[0].AllowProbableDuplicates);
    REQUIRE_FALSE(importer.Calls[0].ForceEpubConversion);

    REQUIRE(result.Entries.size() == 4);
    REQUIRE(result.ImportedCount() == 1);

    REQUIRE(result.Entries[0].ArchivePath == std::filesystem::path("folder/first.fb2"));
    REQUIRE(result.Entries[0].Status == Librova::ZipImporting::EZipEntryImportStatus::Imported);
    REQUIRE(result.Entries[1].ArchivePath == std::filesystem::path("second.fb2"));
    REQUIRE(result.Entries[1].Status == Librova::ZipImporting::EZipEntryImportStatus::Skipped);
    REQUIRE(result.Entries[1].SingleFileResult.has_value());
    REQUIRE(result.Entries[1].SingleFileResult->Status == Librova::Importing::ESingleFileImportStatus::RejectedDuplicate);
    REQUIRE(result.Entries[2].ArchivePath == std::filesystem::path("notes.txt"));
    REQUIRE(result.Entries[2].Status == Librova::ZipImporting::EZipEntryImportStatus::UnsupportedEntry);
    REQUIRE(result.Entries[3].ArchivePath == std::filesystem::path("nested/archive.zip"));
    REQUIRE(result.Entries[3].Status == Librova::ZipImporting::EZipEntryImportStatus::NestedArchiveSkipped);

    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "extracted" / "folder" / "first.fb2"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "extracted" / "second.fb2"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "extracted"));
}

TEST_CASE("ZIP import coordinator forwards forced EPUB conversion to extracted entries", "[zip-import]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-zip-import-force-convert");
    const std::filesystem::path zipPath = CreateZipFixture(sandbox.GetPath() / "books.zip");
    CStubSingleFileImporter importer;
    CTestProgressSink progressSink;

    const Librova::ZipImporting::CZipImportCoordinator coordinator(importer);
    const auto result = coordinator.Run({
        .ZipPath = zipPath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .ForceEpubConversion = true
    }, progressSink, {});

    REQUIRE(result.Entries.size() == 4);
    REQUIRE(importer.Calls.size() == 2);
    REQUIRE(importer.Calls[0].ForceEpubConversion);
    REQUIRE(importer.Calls[1].ForceEpubConversion);
}

TEST_CASE("ZIP import coordinator forwards writer repository override to extracted entries", "[zip-import]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-zip-import-writer-repository");
    const std::filesystem::path zipPath = CreateZipFixture(sandbox.GetPath() / "books.zip");
    CStubSingleFileImporter importer;
    CStubBookRepository writerRepository;
    CTestProgressSink progressSink;

    const Librova::ZipImporting::CZipImportCoordinator coordinator(importer);
    const auto result = coordinator.Run({
        .ZipPath = zipPath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .WriterRepository = std::ref(writerRepository)
    }, progressSink, {});

    REQUIRE(result.Entries.size() == 4);
    REQUIRE(importer.Calls.size() == 2);
    REQUIRE(importer.Calls[0].RepositoryOverride.has_value());
    REQUIRE(importer.Calls[1].RepositoryOverride.has_value());
    REQUIRE(&importer.Calls[0].RepositoryOverride->get() == &writerRepository);
    REQUIRE(&importer.Calls[1].RepositoryOverride->get() == &writerRepository);
}

TEST_CASE("ZIP import coordinator cleans per-entry working directories after import", "[zip-import]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-zip-import-entry-cleanup");
    const std::filesystem::path zipPath = CreateZipFixture(sandbox.GetPath() / "books.zip");
    CStubSingleFileImporter importer;
    CTestProgressSink progressSink;

    const Librova::ZipImporting::CZipImportCoordinator coordinator(importer);
    const auto result = coordinator.Run({
        .ZipPath = zipPath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .AllowProbableDuplicates = true
    }, progressSink, {});

    REQUIRE(result.Entries.size() == 4);
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "0" / "covers"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "0"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "1" / "covers"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "1"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries"));
}

TEST_CASE("ZIP import coordinator removes non-empty per-entry working directories after import", "[zip-import]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-zip-import-non-empty-entry-cleanup");
    const std::filesystem::path zipPath = CreateZipFixture(sandbox.GetPath() / "books.zip");
    CWorkspaceWritingSingleFileImporter importer;
    CTestProgressSink progressSink;

    const Librova::ZipImporting::CZipImportCoordinator coordinator(importer);
    const auto result = coordinator.Run({
        .ZipPath = zipPath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .AllowProbableDuplicates = true
    }, progressSink, {});

    REQUIRE(result.Entries.size() == 4);
    REQUIRE(importer.Calls.size() == 2);
    REQUIRE(importer.Calls[0].WorkingDirectory == sandbox.GetPath() / "work" / "entries" / "0");
    REQUIRE(importer.Calls[1].WorkingDirectory == sandbox.GetPath() / "work" / "entries" / "1");
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "0" / "covers"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "0"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "1" / "covers"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "1"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries"));
}

TEST_CASE("ZIP import coordinator removes empty cover workspaces for numeric entry names", "[zip-import]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-zip-import-empty-numeric-entry-cleanup");
    const std::filesystem::path zipPath = CreateNumericZipFixture(sandbox.GetPath() / "books.zip");
    CEmptyWorkspaceSingleFileImporter importer;
    CTestProgressSink progressSink;

    const Librova::ZipImporting::CZipImportCoordinator coordinator(importer);
    const auto result = coordinator.Run({
        .ZipPath = zipPath,
        .WorkingDirectory = sandbox.GetPath() / "work",
        .AllowProbableDuplicates = true
    }, progressSink, {});

    REQUIRE(result.Entries.size() == 2);
    REQUIRE(importer.Calls.size() == 2);
    REQUIRE(importer.Calls[0].WorkingDirectory == sandbox.GetPath() / "work" / "entries" / "0");
    REQUIRE(importer.Calls[1].WorkingDirectory == sandbox.GetPath() / "work" / "entries" / "1");
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "0" / "covers"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "0"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "1" / "covers"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "1"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries"));
}

TEST_CASE("ZIP import coordinator rejects unsafe archive entry paths", "[zip-import]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-zip-import-unsafe");
    const std::filesystem::path zipPath = CreateUnsafeZipFixture(sandbox.GetPath() / "unsafe.zip");
    CStubSingleFileImporter importer;
    CTestProgressSink progressSink;

    const Librova::ZipImporting::CZipImportCoordinator coordinator(importer);
    const auto result = coordinator.Run({
        .ZipPath = zipPath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.Entries.size() == 2);
    REQUIRE(result.Entries[0].ArchivePath == std::filesystem::path("../outside.fb2"));
    REQUIRE(result.Entries[0].Status == Librova::ZipImporting::EZipEntryImportStatus::UnsupportedEntry);
    REQUIRE(result.Entries[0].Error == "Unsafe ZIP entry path.");
    REQUIRE(result.Entries[1].Status == Librova::ZipImporting::EZipEntryImportStatus::Imported);
    REQUIRE(importer.Calls.size() == 1);
    REQUIRE(importer.Calls[0].SourcePath.filename() == "inside.fb2");
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "outside.fb2"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "extracted" / "safe" / "inside.fb2"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "extracted"));
}

TEST_CASE("ZIP import coordinator opens Unicode archive paths on Windows", "[zip-import]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-zip-import-unicode");
    const std::filesystem::path zipPath = CreateZipFixture(sandbox.GetPath() / std::filesystem::path{u8"книги.zip"});
    CStubSingleFileImporter importer;
    CTestProgressSink progressSink;

    const Librova::ZipImporting::CZipImportCoordinator coordinator(importer);
    const auto result = coordinator.Run({
        .ZipPath = zipPath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.ImportedCount() == 1);
    REQUIRE(importer.Calls.size() == 2);
}

TEST_CASE("ZIP import coordinator logs skipped and failed entries into host log", "[zip-import][logging]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-zip-import-logging");
    const std::filesystem::path zipPath = CreateZipFixture(sandbox.GetPath() / "books.zip");
    const auto logPath = sandbox.GetPath() / "Logs" / "host.log";
    CStubSingleFileImporter importer;
    CTestProgressSink progressSink;

    Librova::Logging::CLogging::InitializeHostLogger(logPath);

    {
        const Librova::ZipImporting::CZipImportCoordinator coordinator(importer);
        const auto result = coordinator.Run({
            .ZipPath = zipPath,
            .WorkingDirectory = sandbox.GetPath() / "work",
            .AllowProbableDuplicates = true
        }, progressSink, {});

        REQUIRE(result.Entries.size() == 4);
    }

    Librova::Logging::CLogging::Shutdown();

    REQUIRE(std::filesystem::exists(logPath));
    const auto logText = ReadTextFile(logPath);
    REQUIRE(logText.find("ZIP entry skipped") != std::string::npos);
    REQUIRE(logText.find("books.zip") != std::string::npos);
    REQUIRE(logText.find("second.fb2") != std::string::npos);
    REQUIRE(logText.find("notes.txt") != std::string::npos);
    REQUIRE(logText.find("nested/archive.zip") != std::string::npos);
    REQUIRE(logText.find("Nested ZIP archives are not supported.") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Cancellation tests
// ---------------------------------------------------------------------------

namespace {

class CCancellingProgressSink final : public Librova::Domain::IProgressSink
{
public:
    void ReportValue(const int, std::string_view) override {}
    bool IsCancellationRequested() const override { return true; }
};

} // namespace

TEST_CASE("ZIP import coordinator stops processing entries when IsCancellationRequested returns true", "[zip-import]")
{
    CScopedDirectory sandbox("librova-zip-cancel-sink");
    const auto zipPath = sandbox.GetPath() / "books.zip";

    {
        int ec = ZIP_ER_OK;
        const auto zipPathUtf8 = Librova::Unicode::PathToUtf8(zipPath);
        zip_t* archive = zip_open(zipPathUtf8.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &ec);
        REQUIRE(archive != nullptr);
        const char data[] = "<fb2/>";
        zip_source_t* src = zip_source_buffer(archive, data, sizeof(data) - 1, 0);
        zip_file_add(archive, "book.fb2", src, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8);
        zip_close(archive);
    }

    CStubSingleFileImporter importer;
    CCancellingProgressSink progressSink;

    const Librova::ZipImporting::CZipImportCoordinator coordinator(importer);
    const auto result = coordinator.Run({
        .ZipPath = zipPath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(importer.Calls.empty());
    REQUIRE_FALSE(result.Entries.empty());
    REQUIRE(result.Entries.back().Status == Librova::ZipImporting::EZipEntryImportStatus::Cancelled);
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "extracted"));
}

TEST_CASE("ZIP import coordinator stops processing entries when stop_token is pre-requested", "[zip-import]")
{
    CScopedDirectory sandbox("librova-zip-cancel-token");
    const auto zipPath = sandbox.GetPath() / "books.zip";

    {
        int ec = ZIP_ER_OK;
        const auto zipPathUtf8 = Librova::Unicode::PathToUtf8(zipPath);
        zip_t* archive = zip_open(zipPathUtf8.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &ec);
        REQUIRE(archive != nullptr);
        const char data[] = "<fb2/>";
        zip_source_t* src = zip_source_buffer(archive, data, sizeof(data) - 1, 0);
        zip_file_add(archive, "book.fb2", src, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8);
        zip_close(archive);
    }

    CStubSingleFileImporter importer;
    CTestProgressSink progressSink;

    std::stop_source stopSource;
    stopSource.request_stop();

    const Librova::ZipImporting::CZipImportCoordinator coordinator(importer);
    const auto result = coordinator.Run({
        .ZipPath = zipPath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, stopSource.get_token());

    REQUIRE(importer.Calls.empty());
    REQUIRE_FALSE(result.Entries.empty());
    REQUIRE(result.Entries.back().Status == Librova::ZipImporting::EZipEntryImportStatus::Cancelled);
}

TEST_CASE("ZIP import coordinator preserves UTF-8 entry names when building filesystem paths", "[zip-import]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-zip-import-cyrillic-entry");
    const std::filesystem::path zipPath = CreateCyrillicZipFixture(sandbox.GetPath() / "books.zip");
    CStubSingleFileImporter importer;
    CTestProgressSink progressSink;

    const Librova::ZipImporting::CZipImportCoordinator coordinator(importer);
    const auto result = coordinator.Run({
        .ZipPath = zipPath,
        .WorkingDirectory = sandbox.GetPath() / "work"
    }, progressSink, {});

    REQUIRE(result.Entries.size() == 1);
    REQUIRE(importer.Calls.size() == 1);
    REQUIRE(importer.Calls[0].SourcePath.filename() == Librova::Unicode::PathFromUtf8("книга.fb2"));
    REQUIRE(result.Entries[0].ArchivePath == Librova::Unicode::PathFromUtf8("папка\\книга.fb2"));
}

TEST_CASE("ZIP import coordinator reports progress for completed entries before the first slot finishes", "[zip-import][progress]")
{
    if (std::thread::hardware_concurrency() < 2U)
    {
        SUCCEED("Parallel ZIP progress test requires at least two hardware threads.");
        return;
    }

    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-zip-import-slow-first-progress");
    const std::filesystem::path zipPath = CreateZipFixture(sandbox.GetPath() / "books.zip");
    CSlowFirstZipSingleFileImporter importer;
    CThreadSafeStructuredProgressSink progressSink;

    const Librova::ZipImporting::CZipImportCoordinator coordinator(importer);
    auto futureResult = std::async(std::launch::async, [&] {
        return coordinator.Run({
            .ZipPath = zipPath,
            .WorkingDirectory = sandbox.GetPath() / "work"
        }, progressSink, {});
    });

    importer.WaitForFastCompletion();

    REQUIRE(progressSink.WaitForPartialImportedProgress());

    const auto snapshotsBeforeSlowRelease = progressSink.SnapshotCopy();
    REQUIRE(std::ranges::any_of(snapshotsBeforeSlowRelease, [](const auto& snapshot) {
        return snapshot.ImportedEntries >= 1 && snapshot.ProcessedEntries < snapshot.TotalEntries;
    }));

    importer.ReleaseSlow();
    const auto result = futureResult.get();

    REQUIRE(result.ImportedCount() == 2);
}
