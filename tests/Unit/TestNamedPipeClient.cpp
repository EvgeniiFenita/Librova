#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
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
#include "PipeClient/NamedPipeClient.hpp"
#include "PipeHost/NamedPipeHost.hpp"

namespace {

std::filesystem::path BuildTestPipePath()
{
    const auto uniqueId = std::to_wstring(
        static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
    return std::filesystem::path{std::wstring{LR"(\\.\pipe\Librova.Client.Test.)"} + uniqueId};
}

class CImmediateSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest&,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token) const override
    {
        progressSink.ReportValue(10, "Parsing");
        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{23}
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

    [[nodiscard]] std::vector<Librova::Domain::SDuplicateMatch> FindDuplicates(const Librova::Domain::SCandidateBook&) const override
    {
        return {};
    }

    [[nodiscard]] Librova::Domain::IBookQueryRepository::SLibraryStatistics GetLibraryStatistics() const override
    {
        return {
            .BookCount = 42,
            .TotalManagedBookSizeBytes = 5ULL * 1024ULL * 1024ULL
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

    [[nodiscard]] std::optional<Librova::Domain::SBook> GetById(const Librova::Domain::SBookId) const override
    {
        return std::nullopt;
    }

    void Remove(const Librova::Domain::SBookId) override
    {
    }
};

std::filesystem::path CreateImportSourcePath()
{
    const auto root = std::filesystem::temp_directory_path() / "librova-pipe-client-import";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto sourcePath = root / "book.fb2";
    std::ofstream(sourcePath).put('x');
    return sourcePath;
}

} // namespace

TEST_CASE("Named pipe client performs typed StartImport call through host", "[pipe-client]")
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
    Librova::PipeTransport::CPipeRequestDispatcher dispatcher(adapter);
    Librova::PipeHost::CNamedPipeHost host(dispatcher);

    const auto pipePath = BuildTestPipePath();
    std::exception_ptr serverFailure;

    std::jthread serverThread([&] {
        try
        {
            Librova::PipeTransport::CNamedPipeServer server(pipePath);
            host.RunSingleSession(server.WaitForClient());
        }
        catch (...)
        {
            serverFailure = std::current_exception();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    Librova::PipeClient::CNamedPipeClient client(pipePath);
    const auto sourcePath = CreateImportSourcePath();

    librova::v1::StartImportRequest request;
    auto* import = request.mutable_import();
    import->add_source_paths(sourcePath.string());
    import->set_working_directory((sourcePath.parent_path() / "work").string());

    const auto response = client.Call<librova::v1::StartImportRequest, librova::v1::StartImportResponse>(
        Librova::PipeTransport::EPipeMethod::StartImport,
        request,
        std::chrono::seconds(2));

    REQUIRE(response.job_id() != 0);

    serverThread.join();
    REQUIRE(serverFailure == nullptr);
}

TEST_CASE("Named pipe client raises transport error on invalid pipe response", "[pipe-client]")
{
    const auto pipePath = BuildTestPipePath();
    std::exception_ptr serverFailure;

    std::jthread serverThread([&] {
        try
        {
            Librova::PipeTransport::CNamedPipeServer server(pipePath);
            auto connection = server.WaitForClient();
            const auto ignoredRequest = connection.ReadMessage();
            (void)ignoredRequest;
            connection.WriteMessage(std::vector<std::byte>{std::byte{0x01}, std::byte{0x02}});
        }
        catch (...)
        {
            serverFailure = std::current_exception();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    Librova::PipeClient::CNamedPipeClient client(pipePath);

    librova::v1::WaitImportJobRequest request;
    request.set_job_id(1);
    request.set_timeout_ms(1);

    const auto failingCall = [&client, &request] {
        return client.Call<librova::v1::WaitImportJobRequest, librova::v1::WaitImportJobResponse>(
            Librova::PipeTransport::EPipeMethod::WaitImportJob,
            request,
            std::chrono::seconds(2));
    };

    REQUIRE_THROWS_AS(failingCall(), Librova::PipeClient::CPipeTransportError);

    serverThread.join();
    REQUIRE(serverFailure == nullptr);
}

TEST_CASE("Named pipe client performs typed GetLibraryStatistics call through host", "[pipe-client]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    const CStatisticsQueryRepository queryRepository;
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository);
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryExportFacade exportFacade(bookRepository, std::filesystem::temp_directory_path());
    Librova::ManagedTrash::CManagedTrashService trashService(std::filesystem::temp_directory_path());
    Librova::Application::CLibraryTrashFacade trashFacade(bookRepository, trashService, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, catalogFacade, exportFacade, trashFacade);
    Librova::PipeTransport::CPipeRequestDispatcher dispatcher(adapter);
    Librova::PipeHost::CNamedPipeHost host(dispatcher);

    const auto pipePath = BuildTestPipePath();
    std::exception_ptr serverFailure;

    std::jthread serverThread([&] {
        try
        {
            Librova::PipeTransport::CNamedPipeServer server(pipePath);
            host.RunSingleSession(server.WaitForClient());
        }
        catch (...)
        {
            serverFailure = std::current_exception();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    Librova::PipeClient::CNamedPipeClient client(pipePath);
    librova::v1::GetLibraryStatisticsRequest request;

    const auto response = client.Call<librova::v1::GetLibraryStatisticsRequest, librova::v1::GetLibraryStatisticsResponse>(
        Librova::PipeTransport::EPipeMethod::GetLibraryStatistics,
        request,
        std::chrono::seconds(2));

    REQUIRE(response.has_statistics());
    REQUIRE(response.statistics().book_count() == 42);
    REQUIRE(response.statistics().total_managed_book_size_bytes() == 5ULL * 1024ULL * 1024ULL);

    serverThread.join();
    REQUIRE(serverFailure == nullptr);
}
