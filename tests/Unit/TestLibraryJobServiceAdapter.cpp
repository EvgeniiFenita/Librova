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
#include "ManagedTrash/ManagedTrashService.hpp"
#include "ProtoServices/LibraryJobServiceAdapter.hpp"

namespace {

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

    [[nodiscard]] std::optional<Librova::Domain::SBook> GetById(const Librova::Domain::SBookId) const override
    {
        return std::nullopt;
    }

    void Remove(const Librova::Domain::SBookId) override
    {
    }
};

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
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    const CEmptyQueryRepository queryRepository;
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository);
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryExportFacade exportFacade(bookRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(bookRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, catalogFacade, exportFacade, trashFacade);
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
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    const CEmptyQueryRepository queryRepository;
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository);
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryExportFacade exportFacade(bookRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(bookRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, catalogFacade, exportFacade, trashFacade);
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
    book.File.ManagedPath = "Books/0000000201/book.epub";
    book.File.SizeBytes = 512;
    book.File.Sha256Hex = "catalog-adapter-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    static_cast<void>(writeRepository.Add(book));

    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, &writeRepository);
    Librova::Application::CLibraryExportFacade exportFacade(writeRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(writeRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, catalogFacade, exportFacade, trashFacade);

    librova::v1::ListBooksRequest request;
    request.mutable_query()->set_text("road");
    request.mutable_query()->set_limit(10);

    const auto response = adapter.ListBooks(request);
    REQUIRE(response.items_size() == 1);
    REQUIRE(response.total_count() == 1);
    REQUIRE(response.items(0).title() == "Roadside Picnic");
    REQUIRE(response.items(0).authors_size() == 1);
    REQUIRE(response.items(0).managed_path() == "Books/0000000201/book.epub");

    std::filesystem::remove(databasePath);
}

TEST_CASE("Library job service adapter exports managed book file over protobuf", "[proto-service][catalog]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-proto-service-export";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Books/0000000202");

    const std::filesystem::path databasePath = sandbox / "librova-proto-service-export.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Roadside Picnic";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Books/0000000202/book.epub";
    book.File.SizeBytes = 512;
    book.File.Sha256Hex = "export-adapter-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    const auto bookId = writeRepository.Add(book);
    std::ofstream(sandbox / "Library/Books/0000000202/book.epub", std::ios::binary) << "epub-export";

    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, &writeRepository);
    Librova::Application::CLibraryExportFacade exportFacade(writeRepository, sandbox / "Library");
    Librova::ManagedTrash::CManagedTrashService trashService(sandbox / "Library");
    Librova::Application::CLibraryTrashFacade trashFacade(writeRepository, trashService, sandbox / "Library");
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, catalogFacade, exportFacade, trashFacade);

    librova::v1::ExportBookRequest request;
    request.set_book_id(bookId.Value);
    request.set_destination_path((sandbox / "Exports" / "RoadsidePicnic.epub").string());

    const auto response = adapter.ExportBook(request);
    REQUIRE(response.has_exported_path());
    REQUIRE(std::filesystem::exists(response.exported_path()));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library job service adapter exposes aggregate library statistics over protobuf", "[proto-service][catalog]")
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
    firstBook.File.ManagedPath = "Books/0000000205/alpha.epub";
    firstBook.File.SizeBytes = 1024;
    firstBook.File.Sha256Hex = "stats-adapter-hash-1";
    firstBook.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    static_cast<void>(writeRepository.Add(firstBook));

    Librova::Domain::SBook secondBook = firstBook;
    secondBook.Metadata.TitleUtf8 = "Beta";
    secondBook.File.ManagedPath = "Books/0000000206/beta.epub";
    secondBook.File.SizeBytes = 2048;
    secondBook.File.Sha256Hex = "stats-adapter-hash-2";
    secondBook.AddedAtUtc += std::chrono::hours{1};
    static_cast<void>(writeRepository.Add(secondBook));

    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, &writeRepository);
    Librova::Application::CLibraryExportFacade exportFacade(writeRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(writeRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, catalogFacade, exportFacade, trashFacade);

    librova::v1::GetLibraryStatisticsRequest request;
    const auto response = adapter.GetLibraryStatistics(request);

    REQUIRE(response.has_statistics());
    REQUIRE(response.statistics().book_count() == 2);
    REQUIRE(response.statistics().total_managed_book_size_bytes() == 3072);

    std::filesystem::remove(databasePath);
}

TEST_CASE("Library job service adapter exports FB2 as EPUB over protobuf when converter is configured", "[proto-service][catalog]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-proto-service-export-converted";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Books/0000000204");

    const std::filesystem::path databasePath = sandbox / "librova-proto-service-export-converted.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Export Converted";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.File.Format = Librova::Domain::EBookFormat::Fb2;
    book.File.ManagedPath = "Books/0000000204/book.fb2";
    book.File.SizeBytes = 512;
    book.File.Sha256Hex = "export-converted-adapter-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    const auto bookId = writeRepository.Add(book);
    std::ofstream(sandbox / "Library/Books/0000000204/book.fb2", std::ios::binary) << "fb2-export";

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
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, &writeRepository);
    Librova::Application::CLibraryExportFacade exportFacade(writeRepository, sandbox / "Library", &converter);
    Librova::ManagedTrash::CManagedTrashService trashService(sandbox / "Library");
    Librova::Application::CLibraryTrashFacade trashFacade(writeRepository, trashService, sandbox / "Library");
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, catalogFacade, exportFacade, trashFacade);

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

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Library job service adapter moves managed book to trash over protobuf", "[proto-service][catalog]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-proto-service-trash";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Books/0000000203");

    const std::filesystem::path databasePath = sandbox / "librova-proto-service-trash.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Trash Roadside Picnic";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Books/0000000203/book.epub";
    book.File.SizeBytes = 512;
    book.File.Sha256Hex = "trash-adapter-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    const auto bookId = writeRepository.Add(book);
    std::ofstream(sandbox / "Library/Books/0000000203/book.epub", std::ios::binary) << "epub-trash";

    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, &writeRepository);
    Librova::Application::CLibraryExportFacade exportFacade(writeRepository, sandbox / "Library");
    Librova::ManagedTrash::CManagedTrashService trashService(sandbox / "Library");
    Librova::Application::CLibraryTrashFacade trashFacade(writeRepository, trashService, sandbox / "Library");
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, catalogFacade, exportFacade, trashFacade);

    librova::v1::MoveBookToTrashRequest request;
    request.set_book_id(bookId.Value);

    const auto response = adapter.MoveBookToTrash(request);
    REQUIRE(response.has_trashed_book_id());
    REQUIRE(response.trashed_book_id() == bookId.Value);
    REQUIRE_FALSE(writeRepository.GetById(bookId).has_value());
    REQUIRE(std::filesystem::exists(sandbox / "Library/Trash/Books/0000000203/book.epub"));

    std::filesystem::remove_all(sandbox);
}
