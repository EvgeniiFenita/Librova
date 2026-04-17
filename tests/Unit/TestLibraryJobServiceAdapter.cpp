#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stop_token>
#include <thread>

#include "Application/LibraryCatalogFacade.hpp"
#include "Application/LibraryExportFacade.hpp"
#include "Application/LibraryImportFacade.hpp"
#include "Application/LibraryTrashFacade.hpp"
#include "ApplicationJobs/ImportJobService.hpp"
#include "BookDatabase/SqliteBookQueryRepository.hpp"
#include "BookDatabase/SqliteBookRepository.hpp"
#include "DatabaseRuntime/SchemaMigrator.hpp"
#include "Jobs/ImportJobManager.hpp"
#include "Jobs/ImportJobRunner.hpp"
#include "Logging/Logging.hpp"
#include "ManagedTrash/ManagedTrashService.hpp"
#include "ProtoServices/LibraryJobServiceAdapter.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace {

void CloseRepositoryAndRemoveDatabase(
    const std::filesystem::path& databasePath,
    Librova::BookDatabase::CSqliteBookRepository& repository)
{
    repository.CloseSession();
    std::filesystem::remove(databasePath);
}

void CloseRepositoryAndRemoveAll(
    const std::filesystem::path& path,
    Librova::BookDatabase::CSqliteBookRepository& repository)
{
    repository.CloseSession();
    std::filesystem::remove_all(path);
}

[[nodiscard]] std::string ReadAllText(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

class CImmediateSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest&,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token) const override
    {
        progressSink.ReportValue(20, "Parsing");
        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{12}
        };
    }
};

class CBlockingSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest&,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token stopToken) const override
    {
        progressSink.ReportValue(25, "Blocking import");

        {
            const std::scoped_lock lock(Mutex);
            Started = true;
        }

        Condition.notify_all();

        while (!stopToken.stop_requested())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Cancelled,
            .Warnings = {"Cancelled by adapter test"}
        };
    }

    bool WaitUntilStarted(const std::chrono::milliseconds timeout) const
    {
        std::unique_lock lock(Mutex);
        return Condition.wait_for(lock, timeout, [this] {
            return Started;
        });
    }

private:
    mutable std::mutex Mutex;
    mutable std::condition_variable Condition;
    mutable bool Started = false;
};

class CEmptyQueryRepository final : public Librova::Domain::IBookQueryRepository
{
public:
    [[nodiscard]] std::vector<Librova::Domain::SBook> Search(const Librova::Domain::SSearchQuery&) const override
    {
        return {};
    }

    [[nodiscard]] std::uint64_t CountSearchResults(const Librova::Domain::SSearchQuery&) const override
    {
        return 0;
    }

    [[nodiscard]] std::vector<std::string> ListAvailableLanguages(const Librova::Domain::SSearchQuery&) const override
    {
        return {};
    }

    [[nodiscard]] std::vector<std::string> ListAvailableTags(const Librova::Domain::SSearchQuery&) const override
    {
        return {};
    }

    [[nodiscard]] std::vector<std::string> ListAvailableGenres(const Librova::Domain::SSearchQuery&) const override
    {
        return {};
    }

    [[nodiscard]] std::vector<Librova::Domain::SDuplicateMatch> FindDuplicates(const Librova::Domain::SCandidateBook&) const override
    {
        return {};
    }

    [[nodiscard]] Librova::Domain::IBookQueryRepository::SLibraryStatistics GetLibraryStatistics() const override
    {
        return {};
    }
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

SImportSandbox CreateImportSandbox(const std::string_view scenario)
{
    const auto root = std::filesystem::temp_directory_path() / ("librova-proto-service-import-" + std::string{scenario});
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto sourcePath = root / "book.fb2";
    std::ofstream(sourcePath).put('x');
    return {
        .Root = root,
        .SourcePath = sourcePath,
        .WorkingDirectory = root / "work"
    };
}

} // namespace

TEST_CASE("Library job service adapter starts jobs and returns protobuf results", "[proto-service]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    const CEmptyQueryRepository queryRepository;
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, GetEmptyBookRepository());
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryExportFacade exportFacade(bookRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(bookRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);
    const auto sandbox = CreateImportSandbox("start");

    librova::v1::StartImportRequest startRequest;
    auto* import = startRequest.mutable_import();
    import->add_source_paths(sandbox.SourcePath.string());
    import->set_working_directory(sandbox.WorkingDirectory.string());
    import->set_allow_probable_duplicates(true);

    const auto startResponse = adapter.StartImport(startRequest);
    REQUIRE(startResponse.job_id() != 0);

    librova::v1::WaitImportJobRequest waitRequest;
    waitRequest.set_job_id(startResponse.job_id());
    waitRequest.set_timeout_ms(1000);

    const auto waitResponse = adapter.WaitImportJob(waitRequest);
    REQUIRE(waitResponse.completed());

    librova::v1::GetImportJobResultRequest resultRequest;
    resultRequest.set_job_id(startResponse.job_id());

    const auto resultResponse = adapter.GetImportJobResult(resultRequest);
    REQUIRE(resultResponse.has_result());
    REQUIRE(resultResponse.result().snapshot().job_id() == startResponse.job_id());
    REQUIRE(resultResponse.result().summary().imported_entries() == 1);
    std::filesystem::remove_all(sandbox.Root);
}

TEST_CASE("Library job service adapter exposes snapshot cancellation and removal", "[proto-service]")
{
    CBlockingSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    const CEmptyQueryRepository queryRepository;
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, GetEmptyBookRepository());
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryExportFacade exportFacade(bookRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(bookRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);
    const auto sandbox = CreateImportSandbox("cancel");

    librova::v1::StartImportRequest startRequest;
    startRequest.mutable_import()->add_source_paths(sandbox.SourcePath.string());
    startRequest.mutable_import()->set_working_directory(sandbox.WorkingDirectory.string());

    const auto startResponse = adapter.StartImport(startRequest);
    REQUIRE(importer.WaitUntilStarted(std::chrono::seconds(1)));

    librova::v1::GetImportJobSnapshotRequest snapshotRequest;
    snapshotRequest.set_job_id(startResponse.job_id());

    const auto snapshotResponse = adapter.GetImportJobSnapshot(snapshotRequest);
    REQUIRE(snapshotResponse.has_snapshot());
    REQUIRE(snapshotResponse.snapshot().status() == librova::v1::IMPORT_JOB_STATUS_RUNNING);

    librova::v1::CancelImportJobRequest cancelRequest;
    cancelRequest.set_job_id(startResponse.job_id());
    REQUIRE(adapter.CancelImportJob(cancelRequest).accepted());

    librova::v1::WaitImportJobRequest waitRequest;
    waitRequest.set_job_id(startResponse.job_id());
    waitRequest.set_timeout_ms(1000);
    REQUIRE(adapter.WaitImportJob(waitRequest).completed());

    librova::v1::RemoveImportJobRequest removeRequest;
    removeRequest.set_job_id(startResponse.job_id());
    REQUIRE(adapter.RemoveImportJob(removeRequest).removed());
    REQUIRE_FALSE(adapter.GetImportJobSnapshot(snapshotRequest).has_snapshot());
    std::filesystem::remove_all(sandbox.Root);
}

TEST_CASE("Library job service adapter exposes structured not-found errors for import job cancellation and removal", "[proto-service]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    const CEmptyQueryRepository queryRepository;
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, GetEmptyBookRepository());
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryExportFacade exportFacade(bookRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(bookRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);

    librova::v1::CancelImportJobRequest cancelRequest;
    cancelRequest.set_job_id(999);
    const auto cancelResponse = adapter.CancelImportJob(cancelRequest);
    REQUIRE_FALSE(cancelResponse.accepted());
    REQUIRE(cancelResponse.has_error());
    REQUIRE(cancelResponse.error().code() == librova::v1::ERROR_CODE_NOT_FOUND);
    REQUIRE(cancelResponse.error().message() == "Import job 999 was not found for cancellation.");

    librova::v1::RemoveImportJobRequest removeRequest;
    removeRequest.set_job_id(999);
    const auto removeResponse = adapter.RemoveImportJob(removeRequest);
    REQUIRE_FALSE(removeResponse.removed());
    REQUIRE(removeResponse.has_error());
    REQUIRE(removeResponse.error().code() == librova::v1::ERROR_CODE_NOT_FOUND);
    REQUIRE(removeResponse.error().message() == "Import job 999 was not found for removal.");
}

TEST_CASE("Library job service adapter validates import sources over protobuf", "[proto-service]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    const CEmptyQueryRepository queryRepository;
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, GetEmptyBookRepository());
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryExportFacade exportFacade(bookRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(bookRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);
    const auto sandbox = CreateImportSandbox("validate");
    const auto unsupportedPath = sandbox.Root / "notes.txt";
    std::ofstream(unsupportedPath).put('x');

    librova::v1::ValidateImportSourcesRequest unsupportedRequest;
    unsupportedRequest.add_source_paths(unsupportedPath.string());

    const auto unsupportedResponse = adapter.ValidateImportSources(unsupportedRequest);
    REQUIRE(unsupportedResponse.has_blocking_message());
    REQUIRE(unsupportedResponse.blocking_message() == "Supported source types are .fb2, .epub, and .zip, or a directory containing them.");

    librova::v1::ValidateImportSourcesRequest mixedRequest;
    mixedRequest.add_source_paths(sandbox.SourcePath.string());
    mixedRequest.add_source_paths(unsupportedPath.string());

    const auto mixedResponse = adapter.ValidateImportSources(mixedRequest);
    REQUIRE_FALSE(mixedResponse.has_blocking_message());

    std::filesystem::remove_all(sandbox.Root);
}

TEST_CASE("Library job service adapter rejects missing path in ValidateImportSources", "[proto-service]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    const CEmptyQueryRepository queryRepository;
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, GetEmptyBookRepository());
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryExportFacade exportFacade(bookRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(bookRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);

    const auto missingPath = std::filesystem::temp_directory_path() / "librova-validate-nonexistent-path" / "no-such-file.fb2";
    std::filesystem::remove_all(missingPath.parent_path());

    librova::v1::ValidateImportSourcesRequest request;
    request.add_source_paths(missingPath.string());

    const auto response = adapter.ValidateImportSources(request);
    REQUIRE(response.has_blocking_message());
    REQUIRE(response.blocking_message() == "A selected source does not exist.");
}

TEST_CASE("Library job service adapter passes empty directory in ValidateImportSources", "[proto-service]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    const CEmptyQueryRepository queryRepository;
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, GetEmptyBookRepository());
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryExportFacade exportFacade(bookRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(bookRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);

    const auto emptyDir = std::filesystem::temp_directory_path() / "librova-validate-empty-dir";
    std::filesystem::remove_all(emptyDir);
    std::filesystem::create_directories(emptyDir);

    librova::v1::ValidateImportSourcesRequest request;
    request.add_source_paths(emptyDir.string());

    const auto response = adapter.ValidateImportSources(request);
    REQUIRE_FALSE(response.has_blocking_message());

    std::filesystem::remove_all(emptyDir);
}

TEST_CASE("Library job service adapter passes Cyrillic directory name in ValidateImportSources", "[proto-service]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    const CEmptyQueryRepository queryRepository;
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, GetEmptyBookRepository());
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryExportFacade exportFacade(bookRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(bookRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);

    // Regression: path must be decoded as UTF-8, not ANSI (fixes Cyrillic directory names)
    const auto cyrillicDir = std::filesystem::temp_directory_path() / u8"librova-validate-\u041a\u043d\u0438\u0433\u0438";
    std::filesystem::remove_all(cyrillicDir);
    std::filesystem::create_directories(cyrillicDir);

    librova::v1::ValidateImportSourcesRequest request;
    request.add_source_paths(Librova::Unicode::PathToUtf8(cyrillicDir));

    const auto response = adapter.ValidateImportSources(request);
    REQUIRE_FALSE(response.has_blocking_message());

    std::filesystem::remove_all(cyrillicDir);
}

TEST_CASE("Library job service adapter exposes book list query over protobuf", "[proto-service][catalog]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-proto-service-catalog.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Roadside Picnic";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.Metadata.TagsUtf8 = {"classic"};
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Objects/44/82/0000000201.book.epub";
    book.File.SizeBytes = 512;
    book.File.Sha256Hex = "catalog-adapter-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    static_cast<void>(writeRepository.Add(book));

    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, writeRepository);
    Librova::Application::CLibraryExportFacade exportFacade(writeRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(writeRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);

    librova::v1::ListBooksRequest request;
    request.mutable_query()->set_text("road");
    request.mutable_query()->set_limit(10);

    const auto response = adapter.ListBooks(request);
    REQUIRE(response.items_size() == 1);
    REQUIRE(response.total_count() == 1);
    REQUIRE(response.has_statistics());
    REQUIRE(response.statistics().book_count() == 1);
    REQUIRE(response.statistics().total_managed_book_size_bytes() == 512);
    REQUIRE(response.items(0).title() == "Roadside Picnic");
    REQUIRE(response.items(0).authors_size() == 1);
    REQUIRE(response.items(0).managed_file_name() == "0000000201.book.epub");

    CloseRepositoryAndRemoveDatabase(databasePath, writeRepository);
}

TEST_CASE("Library job service adapter exports managed book file over protobuf", "[proto-service][catalog]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-proto-service-export";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/fd/86");

    const std::filesystem::path databasePath = sandbox / "librova-proto-service-export.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Roadside Picnic";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Objects/fd/86/0000000202.book.epub";
    book.File.SizeBytes = 512;
    book.File.Sha256Hex = "export-adapter-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    const auto bookId = writeRepository.Add(book);
    std::ofstream(sandbox / "Library/Objects/fd/86/0000000202.book.epub", std::ios::binary) << "epub-export";

    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, writeRepository);
    Librova::Application::CLibraryExportFacade exportFacade(writeRepository, sandbox / "Library");
    Librova::ManagedTrash::CManagedTrashService trashService(sandbox / "Library");
    Librova::Application::CLibraryTrashFacade trashFacade(writeRepository, trashService, sandbox / "Library");
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);

    librova::v1::ExportBookRequest request;
    request.set_book_id(bookId.Value);
    request.set_destination_path((sandbox / "Exports" / "RoadsidePicnic.epub").string());

    const auto response = adapter.ExportBook(request);
    REQUIRE(response.has_exported_path());
    REQUIRE(std::filesystem::exists(response.exported_path()));

    CloseRepositoryAndRemoveAll(sandbox, writeRepository);
}

TEST_CASE("Library job service adapter returns aggregate statistics inside ListBooks response", "[proto-service][catalog]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-proto-service-statistics.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook firstBook;
    firstBook.Metadata.TitleUtf8 = "Alpha";
    firstBook.Metadata.AuthorsUtf8 = {"Author One"};
    firstBook.Metadata.Language = "en";
    firstBook.File.Format = Librova::Domain::EBookFormat::Epub;
    firstBook.File.ManagedPath = "Objects/f8/7b/0000000205.book.epub";
    firstBook.File.SizeBytes = 1024;
    firstBook.File.Sha256Hex = "stats-adapter-hash-1";
    firstBook.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    static_cast<void>(writeRepository.Add(firstBook));

    Librova::Domain::SBook secondBook = firstBook;
    secondBook.Metadata.TitleUtf8 = "Beta";
    secondBook.File.ManagedPath = "Objects/b1/80/0000000206.book.epub";
    secondBook.File.SizeBytes = 2048;
    secondBook.File.Sha256Hex = "stats-adapter-hash-2";
    secondBook.AddedAtUtc += std::chrono::hours{1};
    static_cast<void>(writeRepository.Add(secondBook));

    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, writeRepository);
    Librova::Application::CLibraryExportFacade exportFacade(writeRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(writeRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);

    librova::v1::ListBooksRequest request;
    request.mutable_query()->set_limit(10);
    const auto response = adapter.ListBooks(request);

    REQUIRE(response.has_statistics());
    REQUIRE(response.statistics().book_count() == 2);
    REQUIRE(response.statistics().total_managed_book_size_bytes() == 3072);
    REQUIRE(response.statistics().total_library_size_bytes() > 3072);

    CloseRepositoryAndRemoveDatabase(databasePath, writeRepository);
}

TEST_CASE("Library job service adapter logs import snapshot wait and missing result outcomes", "[proto-service][logging]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-proto-service-logging";
    std::filesystem::remove_all(sandbox);
    const auto logPath = sandbox / "Logs" / "host.log";

    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    const CEmptyQueryRepository queryRepository;
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, GetEmptyBookRepository());
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryExportFacade exportFacade(bookRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(bookRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);

    Librova::Logging::CLogging::InitializeHostLogger(logPath);
    try
    {
        librova::v1::GetImportJobSnapshotRequest snapshotRequest;
        snapshotRequest.set_job_id(404);
        static_cast<void>(adapter.GetImportJobSnapshot(snapshotRequest));

        librova::v1::WaitImportJobRequest waitRequest;
        waitRequest.set_job_id(404);
        waitRequest.set_timeout_ms(1);
        const auto waitResponse = adapter.WaitImportJob(waitRequest);
        REQUIRE_FALSE(waitResponse.completed());

        librova::v1::GetImportJobResultRequest resultRequest;
        resultRequest.set_job_id(404);
        static_cast<void>(adapter.GetImportJobResult(resultRequest));

        Librova::Logging::CLogging::Shutdown();
    }
    catch (...)
    {
        Librova::Logging::CLogging::Shutdown();
        throw;
    }

    const auto logText = ReadAllText(logPath);
    REQUIRE(logText.find("GetImportJobSnapshot requested unknown job 404.") != std::string::npos);
    // WaitImportJob completed=false is logged at Debug level and must NOT appear in the Info-level log file.
    REQUIRE(logText.find("WaitImportJob for job 404 completed=false.") == std::string::npos);
    REQUIRE(logText.find("GetImportJobResult requested unknown job 404.") != std::string::npos);

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library job service adapter logs genre alongside language for ListBooks", "[proto-service][logging][catalog]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-proto-service-list-books-logging";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);

    const auto logPath = sandbox / "host.log";
    const auto databasePath = sandbox / "catalog.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Roadside Picnic";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.Metadata.GenresUtf8 = {"sci-fi"};
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Objects/3f/1f/0000000301.book.epub";
    book.File.SizeBytes = 512;
    book.File.Sha256Hex = "list-books-logging-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    static_cast<void>(writeRepository.Add(book));

    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        writeRepository,
        {.LibraryRoot = sandbox});
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, writeRepository);
    Librova::Application::CLibraryExportFacade exportFacade(writeRepository, sandbox);
    Librova::ManagedTrash::CManagedTrashService trashService(sandbox);
    Librova::Application::CLibraryTrashFacade trashFacade(writeRepository, trashService, sandbox);
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);

    try
    {
        Librova::Logging::CLogging::InitializeHostLogger(logPath);

        librova::v1::ListBooksRequest request;
        auto* query = request.mutable_query();
        query->set_text("road");
        query->add_languages("en");
        query->add_genres("sci-fi");
        query->set_limit(10);

        const auto response = adapter.ListBooks(request);
        REQUIRE(response.items_size() == 1);

        Librova::Logging::CLogging::Shutdown();
    }
    catch (...)
    {
        Librova::Logging::CLogging::Shutdown();
        CloseRepositoryAndRemoveAll(sandbox, writeRepository);
        throw;
    }

    const auto logText = ReadAllText(logPath);
    REQUIRE(logText.find("Languages=1") != std::string::npos);
    REQUIRE(logText.find("Genres=1") != std::string::npos);

    CloseRepositoryAndRemoveAll(sandbox, writeRepository);
}

TEST_CASE("Library job service adapter exports FB2 as EPUB over protobuf when converter is configured", "[proto-service][catalog]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-proto-service-export-converted";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/8b/7d");

    const std::filesystem::path databasePath = sandbox / "librova-proto-service-export-converted.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Export Converted";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.File.Format = Librova::Domain::EBookFormat::Fb2;
    book.File.ManagedPath = "Objects/8b/7d/0000000204.book.fb2";
    book.File.SizeBytes = 512;
    book.File.Sha256Hex = "export-converted-adapter-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    const auto bookId = writeRepository.Add(book);
    std::ofstream(sandbox / "Library/Objects/8b/7d/0000000204.book.fb2", std::ios::binary) << "fb2-export";

    class CStubBookConverter final : public Librova::Domain::IBookConverter
    {
    public:
        [[nodiscard]] bool CanConvert(
            const Librova::Domain::EBookFormat sourceFormat,
            const Librova::Domain::EBookFormat destinationFormat) const override
        {
            return sourceFormat == Librova::Domain::EBookFormat::Fb2
                && destinationFormat == Librova::Domain::EBookFormat::Epub;
        }

        [[nodiscard]] Librova::Domain::SConversionResult Convert(
            const Librova::Domain::SConversionRequest& request,
            Librova::Domain::IProgressSink&,
            std::stop_token) const override
        {
            if (!request.DestinationPath.parent_path().empty())
            {
                std::filesystem::create_directories(request.DestinationPath.parent_path());
            }

            std::ofstream(request.DestinationPath, std::ios::binary) << "converted-export";
            return {
                .Status = Librova::Domain::EConversionStatus::Succeeded,
                .OutputPath = request.DestinationPath
            };
        }
    } converter;

    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, writeRepository);
    Librova::Application::CLibraryExportFacade exportFacade(writeRepository, sandbox / "Library", &converter);
    Librova::ManagedTrash::CManagedTrashService trashService(sandbox / "Library");
    Librova::Application::CLibraryTrashFacade trashFacade(writeRepository, trashService, sandbox / "Library");
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);

    librova::v1::ExportBookRequest request;
    request.set_book_id(bookId.Value);
    request.set_destination_path((sandbox / "Exports" / "ExportConverted.epub").string());
    request.set_export_format(librova::v1::BOOK_FORMAT_EPUB);

    const auto response = adapter.ExportBook(request);
    REQUIRE(response.has_exported_path());
    REQUIRE(std::filesystem::exists(response.exported_path()));

    {
        std::ifstream exported(response.exported_path(), std::ios::binary);
        const std::string text{std::istreambuf_iterator<char>(exported), std::istreambuf_iterator<char>()};
        REQUIRE(text == "converted-export");
    }

    CloseRepositoryAndRemoveAll(sandbox, writeRepository);
}

TEST_CASE("Library job service adapter exposes structured validation error for invalid export destination", "[proto-service][catalog]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    const CEmptyQueryRepository queryRepository;
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, GetEmptyBookRepository());
    Librova::Application::CLibraryExportFacade exportFacade(bookRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(bookRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);

    librova::v1::ExportBookRequest request;
    request.set_book_id(1);
    request.set_destination_path("relative.epub");

    const auto response = adapter.ExportBook(request);
    REQUIRE(response.has_error());
    REQUIRE(response.error().code() == librova::v1::ERROR_CODE_VALIDATION);
    REQUIRE(response.error().message() == "Export destination path must be absolute.");
}

TEST_CASE("Library job service adapter exposes structured converter unavailable error for export conversion", "[proto-service][catalog]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-proto-service-export-converter-unavailable";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/1e/7f");

    const std::filesystem::path databasePath = sandbox / "librova-proto-service-export-converter-unavailable.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Export Me As EPUB";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.File.Format = Librova::Domain::EBookFormat::Fb2;
    book.File.ManagedPath = "Objects/1e/7f/0000000207.book.fb2";
    book.File.SizeBytes = 512;
    book.File.Sha256Hex = "export-converter-unavailable-adapter-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    const auto bookId = writeRepository.Add(book);
    std::ofstream(sandbox / "Library/Objects/1e/7f/0000000207.book.fb2", std::ios::binary) << "fb2-export";

    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        writeRepository,
        {.LibraryRoot = sandbox / "Library"});
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, writeRepository);
    Librova::Application::CLibraryExportFacade exportFacade(writeRepository, sandbox / "Library");
    Librova::ManagedTrash::CManagedTrashService trashService(sandbox / "Library");
    Librova::Application::CLibraryTrashFacade trashFacade(writeRepository, trashService, sandbox / "Library");
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);

    librova::v1::ExportBookRequest request;
    request.set_book_id(bookId.Value);
    request.set_destination_path((sandbox / "Exports" / "ExportMeAsEpub.epub").string());
    request.set_export_format(librova::v1::BOOK_FORMAT_EPUB);

    const auto response = adapter.ExportBook(request);
    REQUIRE(response.has_error());
    REQUIRE(response.error().code() == librova::v1::ERROR_CODE_CONVERTER_UNAVAILABLE);
    REQUIRE(response.error().message() == "Configured FB2 to EPUB export converter is unavailable.");

    CloseRepositoryAndRemoveAll(sandbox, writeRepository);
}

TEST_CASE("Library job service adapter exposes structured converter failed error for export conversion", "[proto-service][catalog]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-proto-service-export-converter-failed";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/3f/77");

    const std::filesystem::path databasePath = sandbox / "librova-proto-service-export-converter-failed.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Export Me Failed";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.File.Format = Librova::Domain::EBookFormat::Fb2;
    book.File.ManagedPath = "Objects/3f/77/0000000208.book.fb2";
    book.File.SizeBytes = 512;
    book.File.Sha256Hex = "export-converter-failed-adapter-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    const auto bookId = writeRepository.Add(book);
    std::ofstream(sandbox / "Library/Objects/3f/77/0000000208.book.fb2", std::ios::binary) << "fb2-export";

    class CFailingBookConverter final : public Librova::Domain::IBookConverter
    {
    public:
        [[nodiscard]] bool CanConvert(
            const Librova::Domain::EBookFormat sourceFormat,
            const Librova::Domain::EBookFormat destinationFormat) const override
        {
            return sourceFormat == Librova::Domain::EBookFormat::Fb2
                && destinationFormat == Librova::Domain::EBookFormat::Epub;
        }

        [[nodiscard]] Librova::Domain::SConversionResult Convert(
            const Librova::Domain::SConversionRequest&,
            Librova::Domain::IProgressSink&,
            std::stop_token) const override
        {
            return {
                .Status = Librova::Domain::EConversionStatus::Failed,
                .Warnings = {"Converter exploded."}
            };
        }
    } converter;

    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        writeRepository,
        {.LibraryRoot = sandbox / "Library"});
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, writeRepository);
    Librova::Application::CLibraryExportFacade exportFacade(writeRepository, sandbox / "Library", &converter);
    Librova::ManagedTrash::CManagedTrashService trashService(sandbox / "Library");
    Librova::Application::CLibraryTrashFacade trashFacade(writeRepository, trashService, sandbox / "Library");
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);

    librova::v1::ExportBookRequest request;
    request.set_book_id(bookId.Value);
    request.set_destination_path((sandbox / "Exports" / "ExportMeFailed.epub").string());
    request.set_export_format(librova::v1::BOOK_FORMAT_EPUB);

    const auto response = adapter.ExportBook(request);
    REQUIRE(response.has_error());
    REQUIRE(response.error().code() == librova::v1::ERROR_CODE_CONVERTER_FAILED);
    REQUIRE(response.error().message() == "Converter exploded.");

    CloseRepositoryAndRemoveAll(sandbox, writeRepository);
}

TEST_CASE("Library job service adapter exposes structured cancellation error for export conversion", "[proto-service][catalog]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-proto-service-export-converter-cancelled";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/ac/75");

    const std::filesystem::path databasePath = sandbox / "librova-proto-service-export-converter-cancelled.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Export Me Cancelled";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.File.Format = Librova::Domain::EBookFormat::Fb2;
    book.File.ManagedPath = "Objects/ac/75/0000000209.book.fb2";
    book.File.SizeBytes = 512;
    book.File.Sha256Hex = "export-converter-cancelled-adapter-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    const auto bookId = writeRepository.Add(book);
    std::ofstream(sandbox / "Library/Objects/ac/75/0000000209.book.fb2", std::ios::binary) << "fb2-export";

    class CCancelledBookConverter final : public Librova::Domain::IBookConverter
    {
    public:
        [[nodiscard]] bool CanConvert(
            const Librova::Domain::EBookFormat sourceFormat,
            const Librova::Domain::EBookFormat destinationFormat) const override
        {
            return sourceFormat == Librova::Domain::EBookFormat::Fb2
                && destinationFormat == Librova::Domain::EBookFormat::Epub;
        }

        [[nodiscard]] Librova::Domain::SConversionResult Convert(
            const Librova::Domain::SConversionRequest&,
            Librova::Domain::IProgressSink&,
            std::stop_token) const override
        {
            return {
                .Status = Librova::Domain::EConversionStatus::Cancelled
            };
        }
    } converter;

    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        writeRepository,
        {.LibraryRoot = sandbox / "Library"});
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, writeRepository);
    Librova::Application::CLibraryExportFacade exportFacade(writeRepository, sandbox / "Library", &converter);
    Librova::ManagedTrash::CManagedTrashService trashService(sandbox / "Library");
    Librova::Application::CLibraryTrashFacade trashFacade(writeRepository, trashService, sandbox / "Library");
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);

    librova::v1::ExportBookRequest request;
    request.set_book_id(bookId.Value);
    request.set_destination_path((sandbox / "Exports" / "ExportMeCancelled.epub").string());
    request.set_export_format(librova::v1::BOOK_FORMAT_EPUB);

    const auto response = adapter.ExportBook(request);
    REQUIRE(response.has_error());
    REQUIRE(response.error().code() == librova::v1::ERROR_CODE_CANCELLATION);
    REQUIRE(response.error().message() == "Export conversion was cancelled.");

    CloseRepositoryAndRemoveAll(sandbox, writeRepository);
}

TEST_CASE("Library job service adapter moves managed book to trash over protobuf", "[proto-service][catalog]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-proto-service-trash";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/6a/85");

    const std::filesystem::path databasePath = sandbox / "librova-proto-service-trash.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Trash Roadside Picnic";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Objects/6a/85/0000000203.book.epub";
    book.File.SizeBytes = 512;
    book.File.Sha256Hex = "trash-adapter-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    const auto bookId = writeRepository.Add(book);
    std::ofstream(sandbox / "Library/Objects/6a/85/0000000203.book.epub", std::ios::binary) << "epub-trash";

    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        writeRepository,
        {.LibraryRoot = sandbox / "Library"});
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, writeRepository);
    Librova::Application::CLibraryExportFacade exportFacade(writeRepository, sandbox / "Library");
    Librova::ManagedTrash::CManagedTrashService trashService(sandbox / "Library");
    Librova::Application::CLibraryTrashFacade trashFacade(writeRepository, trashService, sandbox / "Library");
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);

    librova::v1::MoveBookToTrashRequest request;
    request.set_book_id(bookId.Value);

    const auto response = adapter.MoveBookToTrash(request);
    REQUIRE(response.has_trashed_book_id());
    REQUIRE(response.trashed_book_id() == bookId.Value);
    REQUIRE(response.destination() == librova::v1::DELETE_DESTINATION_MANAGED_TRASH);
    REQUIRE_FALSE(response.has_orphaned_files());
    REQUIRE_FALSE(writeRepository.GetById(bookId).has_value());
    REQUIRE(std::filesystem::exists(sandbox / "Library/Trash/Objects/6a/85/0000000203.book.epub"));

    CloseRepositoryAndRemoveAll(sandbox, writeRepository);
}
