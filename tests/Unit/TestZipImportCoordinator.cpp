#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <zip.h>

#include "Importing/SingleFileImportCoordinator.hpp"
#include "Logging/Logging.hpp"
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

class CStubSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest& request,
        Librova::Domain::IProgressSink&,
        std::stop_token) const override
    {
        Calls.push_back(request);

        if (request.SourcePath.filename() == "second.fb2")
        {
            return {
                .Status = Librova::Importing::ESingleFileImportStatus::RejectedDuplicate,
                .Warnings = {"Duplicate"}
            };
        }

        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{static_cast<std::int64_t>(Calls.size())}
        };
    }

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

class CEmptyWorkspaceSingleFileImporter final : public Librova::Importing::ISingleFileImporter
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
        std::filesystem::remove(request.WorkingDirectory / "covers" / "cover.jpg");

        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{static_cast<std::int64_t>(Calls.size())}
        };
    }

    mutable std::vector<Librova::Importing::SSingleFileImportRequest> Calls;
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
        .AllowProbableDuplicates = true
    }, progressSink, {});

    REQUIRE(importer.Calls.size() == 2);
    REQUIRE(importer.Calls[0].SourcePath.filename() == "first.fb2");
    REQUIRE(importer.Calls[1].SourcePath.filename() == "second.fb2");
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
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "first" / "covers"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "first"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "second" / "covers"));
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
    REQUIRE(importer.Calls[0].WorkingDirectory == sandbox.GetPath() / "work" / "entries" / "first");
    REQUIRE(importer.Calls[1].WorkingDirectory == sandbox.GetPath() / "work" / "entries" / "second");
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "first" / "covers"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "first"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "second" / "covers"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "second"));
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
    REQUIRE(importer.Calls[0].WorkingDirectory == sandbox.GetPath() / "work" / "entries" / "689669");
    REQUIRE(importer.Calls[1].WorkingDirectory == sandbox.GetPath() / "work" / "entries" / "689670");
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "689669" / "covers"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "689669"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "689670" / "covers"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox.GetPath() / "work" / "entries" / "689670"));
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
