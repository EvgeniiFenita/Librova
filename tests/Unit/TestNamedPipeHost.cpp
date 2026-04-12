#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <condition_variable>
#include <exception>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>

#include "Application/LibraryCatalogFacade.hpp"
#include "Application/LibraryExportFacade.hpp"
#include "Application/LibraryImportFacade.hpp"
#include "Application/LibraryTrashFacade.hpp"
#include "ApplicationJobs/ImportJobService.hpp"
#include "Jobs/ImportJobManager.hpp"
#include "Jobs/ImportJobRunner.hpp"
#include "ManagedTrash/ManagedTrashService.hpp"
#include "PipeHost/NamedPipeHost.hpp"
#include "TestNamedPipeReadySignal.hpp"

namespace {

std::filesystem::path BuildTestPipePath()
{
    const auto uniqueId = std::to_wstring(
        static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
    return std::filesystem::path{std::wstring{LR"(\\.\pipe\Librova.Host.Test.)"} + uniqueId};
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
            .ImportedBookId = Librova::Domain::SBookId{17}
        };
    }
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

class CStatisticsQueryRepository final : public Librova::Domain::IBookQueryRepository
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
        return {
            .BookCount = 7,
            .TotalManagedBookSizeBytes = 2ULL * 1024ULL * 1024ULL,
            .TotalLibrarySizeBytes = 3ULL * 1024ULL * 1024ULL
        };
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

std::filesystem::path CreateImportSourcePath()
{
    const auto root = std::filesystem::temp_directory_path() / "librova-pipe-host-import";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto sourcePath = root / "book.fb2";
    std::ofstream(sourcePath).put('x');
    return sourcePath;
}

} // namespace

TEST_CASE("Named pipe host serves a protobuf StartImport request end-to-end", "[pipe-host]")
{
    const auto sourcePath = CreateImportSourcePath();
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    const CEmptyQueryRepository queryRepository;
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        bookRepository,
        {.LibraryRoot = sourcePath.parent_path()});
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, bookRepository);
    Librova::Application::CLibraryExportFacade exportFacade(bookRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(bookRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);
    Librova::PipeTransport::CPipeRequestDispatcher dispatcher(adapter);
    Librova::PipeHost::CNamedPipeHost host(dispatcher);

    const auto pipePath = BuildTestPipePath();
    std::exception_ptr serverFailure;
    CTestNamedPipeReadySignal readySignal;

    std::jthread serverThread([&] {
        try
        {
            Librova::PipeTransport::CNamedPipeServer server(pipePath);
            readySignal.NotifyReady();
            host.RunSingleSession(server.WaitForClient());
        }
        catch (...)
        {
            const std::exception_ptr failure = std::current_exception();
            readySignal.NotifyFailure(failure);
            serverFailure = failure;
        }
    });

    readySignal.Wait();

    auto client = Librova::PipeTransport::ConnectToNamedPipe(pipePath, std::chrono::seconds(2));
    librova::v1::StartImportRequest typedRequest;
    auto* import = typedRequest.mutable_import();
    import->add_source_paths(sourcePath.string());
    import->set_working_directory((sourcePath.parent_path() / "work").string());

    std::string payload;
    REQUIRE(typedRequest.SerializeToString(&payload));

    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 5001,
        .Method = Librova::PipeTransport::EPipeMethod::StartImport,
        .Payload = std::move(payload)
    };

    client.WriteMessage(Librova::PipeTransport::SerializeRequestEnvelope(request));

    const auto responseBytes = client.ReadMessage();
    const auto parsedResponse = Librova::PipeTransport::DeserializeResponseEnvelope(responseBytes);
    REQUIRE(parsedResponse.HasValue());
    REQUIRE(parsedResponse.Value->RequestId == request.RequestId);
    REQUIRE(parsedResponse.Value->Status == Librova::PipeTransport::EPipeResponseStatus::Ok);

    librova::v1::StartImportResponse typedResponse;
    REQUIRE(typedResponse.ParseFromString(parsedResponse.Value->Payload));
    REQUIRE(typedResponse.job_id() != 0);

    serverThread.join();
    REQUIRE(serverFailure == nullptr);
}

TEST_CASE("Named pipe host serves catalog statistics inside ListBooks response end-to-end", "[pipe-host]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        GetEmptyBookRepository(),
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    const CStatisticsQueryRepository queryRepository;
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, GetEmptyBookRepository());
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryExportFacade exportFacade(bookRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(bookRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, facade, catalogFacade, exportFacade, trashFacade);
    Librova::PipeTransport::CPipeRequestDispatcher dispatcher(adapter);
    Librova::PipeHost::CNamedPipeHost host(dispatcher);

    const auto pipePath = BuildTestPipePath();
    std::exception_ptr serverFailure;
    CTestNamedPipeReadySignal readySignal;

    std::jthread serverThread([&] {
        try
        {
            Librova::PipeTransport::CNamedPipeServer server(pipePath);
            readySignal.NotifyReady();
            host.RunSingleSession(server.WaitForClient());
        }
        catch (...)
        {
            const std::exception_ptr failure = std::current_exception();
            readySignal.NotifyFailure(failure);
            serverFailure = failure;
        }
    });

    readySignal.Wait();

    auto client = Librova::PipeTransport::ConnectToNamedPipe(pipePath, std::chrono::seconds(2));

    librova::v1::ListBooksRequest typedRequest;
    typedRequest.mutable_query()->set_limit(10);

    std::string payload;
    REQUIRE(typedRequest.SerializeToString(&payload));

    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 5002,
        .Method = Librova::PipeTransport::EPipeMethod::ListBooks,
        .Payload = std::move(payload)
    };

    client.WriteMessage(Librova::PipeTransport::SerializeRequestEnvelope(request));

    const auto responseBytes = client.ReadMessage();
    const auto parsedResponse = Librova::PipeTransport::DeserializeResponseEnvelope(responseBytes);
    REQUIRE(parsedResponse.HasValue());
    REQUIRE(parsedResponse.Value->RequestId == request.RequestId);
    REQUIRE(parsedResponse.Value->Status == Librova::PipeTransport::EPipeResponseStatus::Ok);

    librova::v1::ListBooksResponse typedResponse;
    REQUIRE(typedResponse.ParseFromString(parsedResponse.Value->Payload));
    REQUIRE(typedResponse.items_size() == 0);
    REQUIRE(typedResponse.has_statistics());
    REQUIRE(typedResponse.statistics().book_count() == 7);
    REQUIRE(typedResponse.statistics().total_library_size_bytes() == 3ULL * 1024ULL * 1024ULL);
    REQUIRE(typedResponse.statistics().total_managed_book_size_bytes() == 2ULL * 1024ULL * 1024ULL);

    serverThread.join();
    REQUIRE(serverFailure == nullptr);
}

TEST_CASE("Named pipe host serves a protobuf ValidateImportSources request end-to-end", "[pipe-host]")
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
    Librova::PipeTransport::CPipeRequestDispatcher dispatcher(adapter);
    Librova::PipeHost::CNamedPipeHost host(dispatcher);

    const auto pipePath = BuildTestPipePath();
    std::exception_ptr serverFailure;
    CTestNamedPipeReadySignal readySignal;

    std::jthread serverThread([&] {
        try
        {
            Librova::PipeTransport::CNamedPipeServer server(pipePath);
            readySignal.NotifyReady();
            host.RunSingleSession(server.WaitForClient());
        }
        catch (...)
        {
            const std::exception_ptr failure = std::current_exception();
            readySignal.NotifyFailure(failure);
            serverFailure = failure;
        }
    });

    readySignal.Wait();

    auto client = Librova::PipeTransport::ConnectToNamedPipe(pipePath, std::chrono::seconds(2));
    const auto root = std::filesystem::temp_directory_path() / "librova-pipe-host-validate";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto unsupportedPath = root / "notes.txt";
    std::ofstream(unsupportedPath).put('x');

    librova::v1::ValidateImportSourcesRequest typedRequest;
    typedRequest.add_source_paths(unsupportedPath.string());

    std::string payload;
    REQUIRE(typedRequest.SerializeToString(&payload));

    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 5003,
        .Method = Librova::PipeTransport::EPipeMethod::ValidateImportSources,
        .Payload = std::move(payload)
    };

    client.WriteMessage(Librova::PipeTransport::SerializeRequestEnvelope(request));

    const auto responseBytes = client.ReadMessage();
    const auto parsedResponse = Librova::PipeTransport::DeserializeResponseEnvelope(responseBytes);
    REQUIRE(parsedResponse.HasValue());
    REQUIRE(parsedResponse.Value->RequestId == request.RequestId);
    REQUIRE(parsedResponse.Value->Status == Librova::PipeTransport::EPipeResponseStatus::Ok);

    librova::v1::ValidateImportSourcesResponse typedResponse;
    REQUIRE(typedResponse.ParseFromString(parsedResponse.Value->Payload));
    REQUIRE(typedResponse.has_blocking_message());
    REQUIRE(typedResponse.blocking_message() == "Supported source types are .fb2, .epub, and .zip, or a directory containing them.");

    serverThread.join();
    REQUIRE(serverFailure == nullptr);
    std::filesystem::remove_all(root);
}
