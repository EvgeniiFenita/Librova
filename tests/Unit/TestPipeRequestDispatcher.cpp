#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <stop_token>
#include <thread>

#include "Application/LibraryCatalogFacade.hpp"
#include "Application/LibraryExportFacade.hpp"
#include "Application/LibraryImportFacade.hpp"
#include "Application/LibraryTrashFacade.hpp"
#include "ApplicationJobs/ImportJobService.hpp"
#include "Database/SqliteBookQueryRepository.hpp"
#include "Database/SqliteBookRepository.hpp"
#include "Database/SchemaMigrator.hpp"
#include "Jobs/ImportJobManager.hpp"
#include "Jobs/ImportJobRunner.hpp"
#include "Storage/ManagedTrashService.hpp"
#include "Transport/PipeRequestDispatcher.hpp"

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

class CImmediateSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest&,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token) const override
    {
        progressSink.ReportValue(30, "Importing");
        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{9}
        };
    }
};

class CEmptyBookRepository final : public Librova::Domain::IBookRepository
{
public:
    [[nodiscard]] Librova::Domain::SBookId ReserveId() override { return {1}; }
    [[nodiscard]] Librova::Domain::SBookId Add(const Librova::Domain::SBook& book) override { return book.Id; }
    [[nodiscard]] Librova::Domain::SBookId ForceAdd(const Librova::Domain::SBook& book) override { return book.Id; }
    [[nodiscard]] std::optional<Librova::Domain::SBook> GetById(const Librova::Domain::SBookId) const override { return std::nullopt; }
    void Remove(const Librova::Domain::SBookId) override {}
};

Librova::Domain::IBookRepository& GetEmptyBookRepository()
{
    static CEmptyBookRepository repository;
    return repository;
}

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

struct SDispatcherHarness
{
    CImmediateSingleFileImporter Importer;
    Librova::ZipImporting::CZipImportCoordinator ZipCoordinator;
    CEmptyQueryRepository QueryRepository;
    CEmptyBookRepository BookRepository;
    Librova::Application::CLibraryImportFacade ImportFacade;
    Librova::Application::CLibraryCatalogFacade CatalogFacade;
    Librova::Application::CLibraryExportFacade ExportFacade;
    Librova::ManagedTrash::CManagedTrashService TrashService;
    Librova::Application::CLibraryTrashFacade TrashFacade;
    Librova::Jobs::CImportJobRunner Runner;
    Librova::Jobs::CImportJobManager Manager;
    Librova::ApplicationJobs::CImportJobService Service;
    Librova::ProtoServices::CLibraryJobServiceAdapter Adapter;
    Librova::PipeTransport::CPipeRequestDispatcher Dispatcher;

    SDispatcherHarness()
        : ZipCoordinator(Importer)
        , ImportFacade(
            Importer,
            ZipCoordinator,
            GetEmptyBookRepository(),
            {.LibraryRoot = std::filesystem::temp_directory_path()})
        , CatalogFacade(QueryRepository, GetEmptyBookRepository())
        , ExportFacade(BookRepository, std::filesystem::temp_directory_path())
        , TrashService(std::filesystem::temp_directory_path())
        , TrashFacade(BookRepository, TrashService, std::filesystem::temp_directory_path())
        , Runner(ImportFacade)
        , Manager(Runner)
        , Service(Manager)
        , Adapter(Service, ImportFacade, CatalogFacade, ExportFacade, TrashFacade)
        , Dispatcher(Adapter)
    {
    }
};

} // namespace

namespace {

std::filesystem::path CreateImportSourcePath(const std::string_view scenario)
{
    const auto root = std::filesystem::temp_directory_path() / ("librova-pipe-dispatch-" + std::string{scenario});
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto sourcePath = root / "book.fb2";
    std::ofstream(sourcePath).put('x');
    return sourcePath;
}

} // namespace

TEST_CASE("Pipe dispatcher executes StartImport through protobuf adapter", "[pipe]")
{
    SDispatcherHarness harness;
    const auto sourcePath = CreateImportSourcePath("start");

    librova::v1::StartImportRequest typedRequest;
    auto* import = typedRequest.mutable_import();
    import->add_source_paths(sourcePath.string());
    import->set_working_directory((sourcePath.parent_path() / "work").string());

    std::string payload;
    REQUIRE(typedRequest.SerializeToString(&payload));

    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 1001,
        .Method = Librova::PipeTransport::EPipeMethod::StartImport,
        .Payload = payload
    };

    const auto response = harness.Dispatcher.Dispatch(request);
    REQUIRE(response.RequestId == request.RequestId);
    REQUIRE(response.Status == Librova::PipeTransport::EPipeResponseStatus::Ok);
    REQUIRE(response.ErrorMessage.empty());

    librova::v1::StartImportResponse typedResponse;
    REQUIRE(typedResponse.ParseFromString(response.Payload));
    REQUIRE(typedResponse.job_id() != 0);
}

TEST_CASE("Pipe dispatcher rejects invalid protobuf payloads", "[pipe]")
{
    SDispatcherHarness harness;

    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 2002,
        .Method = Librova::PipeTransport::EPipeMethod::StartImport,
        .Payload = "not protobuf"
    };

    const auto response = harness.Dispatcher.Dispatch(request);
    REQUIRE(response.RequestId == request.RequestId);
    REQUIRE(response.Status == Librova::PipeTransport::EPipeResponseStatus::InvalidRequest);
    REQUIRE_FALSE(response.ErrorMessage.empty());
}

TEST_CASE("Pipe dispatcher executes ValidateImportSources through protobuf adapter", "[pipe]")
{
    SDispatcherHarness harness;
    const auto root = std::filesystem::temp_directory_path() / "librova-pipe-dispatch-validate";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto unsupportedPath = root / "notes.txt";
    std::ofstream(unsupportedPath).put('x');

    librova::v1::ValidateImportSourcesRequest typedRequest;
    typedRequest.add_source_paths(unsupportedPath.string());

    std::string payload;
    REQUIRE(typedRequest.SerializeToString(&payload));

    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 2003,
        .Method = Librova::PipeTransport::EPipeMethod::ValidateImportSources,
        .Payload = payload
    };

    const auto response = harness.Dispatcher.Dispatch(request);
    REQUIRE(response.RequestId == request.RequestId);
    REQUIRE(response.Status == Librova::PipeTransport::EPipeResponseStatus::Ok);

    librova::v1::ValidateImportSourcesResponse typedResponse;
    REQUIRE(typedResponse.ParseFromString(response.Payload));
    REQUIRE(typedResponse.has_blocking_message());
    REQUIRE(typedResponse.blocking_message() == "Supported source types are .fb2, .epub, and .zip, or a directory containing them.");

    std::filesystem::remove_all(root);
}

TEST_CASE("Pipe dispatcher returns UnknownMethod for out-of-range method id", "[pipe]")
{
    SDispatcherHarness harness;

    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 2999,
        .Method = static_cast<Librova::PipeTransport::EPipeMethod>(9999),
        .Payload = {}
    };

    const auto response = harness.Dispatcher.Dispatch(request);
    REQUIRE(response.RequestId == request.RequestId);
    REQUIRE(response.Status == Librova::PipeTransport::EPipeResponseStatus::UnknownMethod);
    REQUIRE(response.ErrorMessage == "Unsupported pipe method.");
}

TEST_CASE("Pipe dispatcher executes ListBooks through protobuf adapter", "[pipe][catalog]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-pipe-dispatch-catalog.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Roadside Picnic";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.File.ManagedPath = "Objects/3f/1f/0000000301.book.epub";
    book.File.SizeBytes = 1024;
    book.File.Sha256Hex = "pipe-catalog-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    static_cast<void>(writeRepository.Add(book));

    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        writeRepository,
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, writeRepository);
    Librova::Application::CLibraryExportFacade exportFacade(writeRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(writeRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);
    Librova::PipeTransport::CPipeRequestDispatcher dispatcher(adapter);

    librova::v1::ListBooksRequest typedRequest;
    typedRequest.mutable_query()->set_text("road");
    typedRequest.mutable_query()->set_limit(10);

    std::string payload;
    REQUIRE(typedRequest.SerializeToString(&payload));

    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 3003,
        .Method = Librova::PipeTransport::EPipeMethod::ListBooks,
        .Payload = payload
    };

    const auto response = dispatcher.Dispatch(request);
    REQUIRE(response.RequestId == request.RequestId);
    REQUIRE(response.Status == Librova::PipeTransport::EPipeResponseStatus::Ok);

    librova::v1::ListBooksResponse typedResponse;
    REQUIRE(typedResponse.ParseFromString(response.Payload));
    REQUIRE(typedResponse.items_size() == 1);
    REQUIRE(typedResponse.items(0).title() == "Roadside Picnic");

    CloseRepositoryAndRemoveDatabase(databasePath, writeRepository);
}

TEST_CASE("Pipe dispatcher executes GetBookDetails through protobuf adapter", "[pipe][catalog]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-pipe-dispatch-details.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Definitely Maybe";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.Metadata.GenresUtf8 = {"sci-fi", "classic"};
    book.Metadata.DescriptionUtf8 = "Aliens land only in one city.";
    book.Metadata.Identifier = "pipe-details-id";
    book.File.Format = Librova::Domain::EBookFormat::Fb2;
    book.File.ManagedPath = "Objects/3f/1f/0000000305.book.fb2";
    book.File.SizeBytes = 2048;
    book.File.Sha256Hex = "pipe-details-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    const auto bookId = writeRepository.Add(book);

    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        writeRepository,
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, writeRepository);
    Librova::Application::CLibraryExportFacade exportFacade(writeRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(writeRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);
    Librova::PipeTransport::CPipeRequestDispatcher dispatcher(adapter);

    librova::v1::GetBookDetailsRequest typedRequest;
    typedRequest.set_book_id(bookId.Value);

    std::string payload;
    REQUIRE(typedRequest.SerializeToString(&payload));

    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 3006,
        .Method = Librova::PipeTransport::EPipeMethod::GetBookDetails,
        .Payload = payload
    };

    const auto response = dispatcher.Dispatch(request);
    REQUIRE(response.RequestId == request.RequestId);
    REQUIRE(response.Status == Librova::PipeTransport::EPipeResponseStatus::Ok);

    librova::v1::GetBookDetailsResponse typedResponse;
    REQUIRE(typedResponse.ParseFromString(response.Payload));
    REQUIRE(typedResponse.has_details());
    REQUIRE(typedResponse.details().title() == "Definitely Maybe");
    REQUIRE(typedResponse.details().genres_size() == 2);
    REQUIRE(typedResponse.details().description() == "Aliens land only in one city.");
    REQUIRE(typedResponse.details().identifier() == "pipe-details-id");

    CloseRepositoryAndRemoveDatabase(databasePath, writeRepository);
}

TEST_CASE("Pipe dispatcher executes ExportBook through protobuf adapter", "[pipe][catalog]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-pipe-dispatch-export";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/e1/0f");

    const std::filesystem::path databasePath = sandbox / "pipe-dispatch-export.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Exported By Dispatcher";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.File.Format = Librova::Domain::EBookFormat::Epub;
    book.File.ManagedPath = "Objects/e1/0f/0000000306.book.epub";
    book.File.SizeBytes = 128;
    book.File.Sha256Hex = "pipe-export-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    const auto bookId = writeRepository.Add(book);
    std::ofstream(sandbox / "Library/Objects/e1/0f/0000000306.book.epub", std::ios::binary) << "dispatcher-export";

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
    Librova::PipeTransport::CPipeRequestDispatcher dispatcher(adapter);

    librova::v1::ExportBookRequest typedRequest;
    typedRequest.set_book_id(bookId.Value);
    typedRequest.set_destination_path((sandbox / "Exports" / "DispatcherExport.epub").string());

    std::string payload;
    REQUIRE(typedRequest.SerializeToString(&payload));

    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 3007,
        .Method = Librova::PipeTransport::EPipeMethod::ExportBook,
        .Payload = payload
    };

    const auto response = dispatcher.Dispatch(request);
    REQUIRE(response.RequestId == request.RequestId);
    REQUIRE(response.Status == Librova::PipeTransport::EPipeResponseStatus::Ok);

    librova::v1::ExportBookResponse typedResponse;
    REQUIRE(typedResponse.ParseFromString(response.Payload));
    REQUIRE(typedResponse.has_exported_path());
    REQUIRE(std::filesystem::exists(typedResponse.exported_path()));

    CloseRepositoryAndRemoveAll(sandbox, writeRepository);
}

TEST_CASE("Pipe dispatcher executes MoveBookToTrash through protobuf adapter", "[pipe][catalog]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-pipe-dispatch-trash";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/d2/20");

    const std::filesystem::path databasePath = sandbox / "pipe-dispatch-trash.db";
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Pipe Trash";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.File.ManagedPath = "Objects/d2/20/0000000302.book.epub";
    book.File.SizeBytes = 128;
    book.File.Sha256Hex = "pipe-trash-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    const auto bookId = writeRepository.Add(book);
    std::ofstream(sandbox / "Library/Objects/d2/20/0000000302.book.epub", std::ios::binary) << "trash";

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
    Librova::PipeTransport::CPipeRequestDispatcher dispatcher(adapter);

    librova::v1::MoveBookToTrashRequest typedRequest;
    typedRequest.set_book_id(bookId.Value);

    std::string payload;
    REQUIRE(typedRequest.SerializeToString(&payload));

    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 3004,
        .Method = Librova::PipeTransport::EPipeMethod::MoveBookToTrash,
        .Payload = payload
    };

    const auto response = dispatcher.Dispatch(request);
    REQUIRE(response.RequestId == request.RequestId);
    REQUIRE(response.Status == Librova::PipeTransport::EPipeResponseStatus::Ok);

    librova::v1::MoveBookToTrashResponse typedResponse;
    REQUIRE(typedResponse.ParseFromString(response.Payload));
    REQUIRE(typedResponse.has_trashed_book_id());
    REQUIRE(typedResponse.trashed_book_id() == bookId.Value);
    REQUIRE(typedResponse.destination() == librova::v1::DELETE_DESTINATION_MANAGED_TRASH);
    REQUIRE_FALSE(typedResponse.has_orphaned_files());
    REQUIRE_FALSE(writeRepository.GetById(bookId).has_value());

    CloseRepositoryAndRemoveAll(sandbox, writeRepository);
}

TEST_CASE("Pipe dispatcher executes snapshot wait result and remove for import jobs", "[pipe][jobs]")
{
    SDispatcherHarness harness;
    const auto sourcePath = CreateImportSourcePath("job-flow");

    librova::v1::StartImportRequest startRequest;
    auto* import = startRequest.mutable_import();
    import->add_source_paths(sourcePath.string());
    import->set_working_directory((sourcePath.parent_path() / "work").string());

    std::string startPayload;
    REQUIRE(startRequest.SerializeToString(&startPayload));
    const auto startResponseEnvelope = harness.Dispatcher.Dispatch({
        .RequestId = 4001,
        .Method = Librova::PipeTransport::EPipeMethod::StartImport,
        .Payload = startPayload
    });

    librova::v1::StartImportResponse startResponse;
    REQUIRE(startResponse.ParseFromString(startResponseEnvelope.Payload));
    REQUIRE(startResponse.job_id() != 0);

    librova::v1::GetImportJobSnapshotRequest snapshotRequest;
    snapshotRequest.set_job_id(startResponse.job_id());
    std::string snapshotPayload;
    REQUIRE(snapshotRequest.SerializeToString(&snapshotPayload));
    const auto snapshotResponseEnvelope = harness.Dispatcher.Dispatch({
        .RequestId = 4002,
        .Method = Librova::PipeTransport::EPipeMethod::GetImportJobSnapshot,
        .Payload = snapshotPayload
    });
    REQUIRE(snapshotResponseEnvelope.Status == Librova::PipeTransport::EPipeResponseStatus::Ok);

    librova::v1::GetImportJobSnapshotResponse snapshotResponse;
    REQUIRE(snapshotResponse.ParseFromString(snapshotResponseEnvelope.Payload));
    REQUIRE(snapshotResponse.has_snapshot());
    REQUIRE(snapshotResponse.snapshot().job_id() == startResponse.job_id());

    librova::v1::WaitImportJobRequest waitRequest;
    waitRequest.set_job_id(startResponse.job_id());
    waitRequest.set_timeout_ms(1000);
    std::string waitPayload;
    REQUIRE(waitRequest.SerializeToString(&waitPayload));
    const auto waitResponseEnvelope = harness.Dispatcher.Dispatch({
        .RequestId = 4003,
        .Method = Librova::PipeTransport::EPipeMethod::WaitImportJob,
        .Payload = waitPayload
    });
    REQUIRE(waitResponseEnvelope.Status == Librova::PipeTransport::EPipeResponseStatus::Ok);

    librova::v1::WaitImportJobResponse waitResponse;
    REQUIRE(waitResponse.ParseFromString(waitResponseEnvelope.Payload));
    REQUIRE(waitResponse.completed());

    librova::v1::GetImportJobResultRequest resultRequest;
    resultRequest.set_job_id(startResponse.job_id());
    std::string resultPayload;
    REQUIRE(resultRequest.SerializeToString(&resultPayload));
    const auto resultResponseEnvelope = harness.Dispatcher.Dispatch({
        .RequestId = 4004,
        .Method = Librova::PipeTransport::EPipeMethod::GetImportJobResult,
        .Payload = resultPayload
    });
    REQUIRE(resultResponseEnvelope.Status == Librova::PipeTransport::EPipeResponseStatus::Ok);

    librova::v1::GetImportJobResultResponse resultResponse;
    REQUIRE(resultResponse.ParseFromString(resultResponseEnvelope.Payload));
    REQUIRE(resultResponse.has_result());
    REQUIRE(resultResponse.result().summary().imported_entries() == 1);

    librova::v1::RemoveImportJobRequest removeRequest;
    removeRequest.set_job_id(startResponse.job_id());
    std::string removePayload;
    REQUIRE(removeRequest.SerializeToString(&removePayload));
    const auto removeResponseEnvelope = harness.Dispatcher.Dispatch({
        .RequestId = 4005,
        .Method = Librova::PipeTransport::EPipeMethod::RemoveImportJob,
        .Payload = removePayload
    });
    REQUIRE(removeResponseEnvelope.Status == Librova::PipeTransport::EPipeResponseStatus::Ok);

    librova::v1::RemoveImportJobResponse removeResponse;
    REQUIRE(removeResponse.ParseFromString(removeResponseEnvelope.Payload));
    REQUIRE(removeResponse.removed());

    std::filesystem::remove_all(sourcePath.parent_path());
}

TEST_CASE("Pipe dispatcher executes cancel import and zero-timeout wait through protobuf adapter", "[pipe][jobs]")
{
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
                .Warnings = {"Cancelled by dispatcher test"}
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
    } importer;

    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CEmptyQueryRepository queryRepository;
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, GetEmptyBookRepository());
    Librova::Application::CLibraryExportFacade exportFacade(bookRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(bookRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);
    Librova::PipeTransport::CPipeRequestDispatcher dispatcher(adapter);

    const auto sourcePath = CreateImportSourcePath("cancel");
    librova::v1::StartImportRequest startRequest;
    startRequest.mutable_import()->add_source_paths(sourcePath.string());
    startRequest.mutable_import()->set_working_directory((sourcePath.parent_path() / "work").string());

    std::string startPayload;
    REQUIRE(startRequest.SerializeToString(&startPayload));
    const auto startResponseEnvelope = dispatcher.Dispatch({
        .RequestId = 4010,
        .Method = Librova::PipeTransport::EPipeMethod::StartImport,
        .Payload = startPayload
    });
    librova::v1::StartImportResponse startResponse;
    REQUIRE(startResponse.ParseFromString(startResponseEnvelope.Payload));
    REQUIRE(importer.WaitUntilStarted(std::chrono::seconds(1)));

    librova::v1::WaitImportJobRequest zeroWaitRequest;
    zeroWaitRequest.set_job_id(startResponse.job_id());
    zeroWaitRequest.set_timeout_ms(0);
    std::string zeroWaitPayload;
    REQUIRE(zeroWaitRequest.SerializeToString(&zeroWaitPayload));
    const auto zeroWaitResponseEnvelope = dispatcher.Dispatch({
        .RequestId = 4011,
        .Method = Librova::PipeTransport::EPipeMethod::WaitImportJob,
        .Payload = zeroWaitPayload
    });
    REQUIRE(zeroWaitResponseEnvelope.Status == Librova::PipeTransport::EPipeResponseStatus::Ok);
    librova::v1::WaitImportJobResponse zeroWaitResponse;
    REQUIRE(zeroWaitResponse.ParseFromString(zeroWaitResponseEnvelope.Payload));
    REQUIRE_FALSE(zeroWaitResponse.completed());

    librova::v1::CancelImportJobRequest cancelRequest;
    cancelRequest.set_job_id(startResponse.job_id());
    std::string cancelPayload;
    REQUIRE(cancelRequest.SerializeToString(&cancelPayload));
    const auto cancelResponseEnvelope = dispatcher.Dispatch({
        .RequestId = 4012,
        .Method = Librova::PipeTransport::EPipeMethod::CancelImportJob,
        .Payload = cancelPayload
    });
    REQUIRE(cancelResponseEnvelope.Status == Librova::PipeTransport::EPipeResponseStatus::Ok);
    librova::v1::CancelImportJobResponse cancelResponse;
    REQUIRE(cancelResponse.ParseFromString(cancelResponseEnvelope.Payload));
    REQUIRE(cancelResponse.accepted());

    std::filesystem::remove_all(sourcePath.parent_path());
}

TEST_CASE("Pipe dispatcher returns catalog statistics inside ListBooks response", "[pipe][catalog]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-pipe-dispatch-statistics.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Statistics";
    book.Metadata.AuthorsUtf8 = {"Author"};
    book.Metadata.Language = "en";
    book.File.ManagedPath = "Objects/65/22/0000000303.book.epub";
    book.File.SizeBytes = 1536;
    book.File.Sha256Hex = "pipe-stats-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    static_cast<void>(writeRepository.Add(book));

    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        writeRepository,
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, writeRepository);
    Librova::Application::CLibraryExportFacade exportFacade(writeRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(writeRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);
    Librova::PipeTransport::CPipeRequestDispatcher dispatcher(adapter);

    librova::v1::ListBooksRequest typedRequest;
    typedRequest.mutable_query()->set_limit(10);

    std::string payload;
    REQUIRE(typedRequest.SerializeToString(&payload));

    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 3005,
        .Method = Librova::PipeTransport::EPipeMethod::ListBooks,
        .Payload = payload
    };

    const auto response = dispatcher.Dispatch(request);
    REQUIRE(response.RequestId == request.RequestId);
    REQUIRE(response.Status == Librova::PipeTransport::EPipeResponseStatus::Ok);

    librova::v1::ListBooksResponse typedResponse;
    REQUIRE(typedResponse.ParseFromString(response.Payload));
    REQUIRE(typedResponse.items_size() == 1);
    REQUIRE(typedResponse.statistics().book_count() == 1);
    REQUIRE(typedResponse.statistics().total_library_size_bytes() > 1536);
    REQUIRE(typedResponse.statistics().total_managed_book_size_bytes() == 1536);

    CloseRepositoryAndRemoveDatabase(databasePath, writeRepository);
}
