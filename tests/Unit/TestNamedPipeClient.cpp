#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <exception>
#include <filesystem>
#include <stop_token>
#include <string>
#include <thread>

#include "Application/LibraryImportFacade.hpp"
#include "ApplicationJobs/ImportJobService.hpp"
#include "Jobs/ImportJobManager.hpp"
#include "Jobs/ImportJobRunner.hpp"
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

} // namespace

TEST_CASE("Named pipe client performs typed StartImport call through host", "[pipe-client]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service);
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

    librova::v1::StartImportRequest request;
    auto* import = request.mutable_import();
    import->set_source_path("C:/books/book.fb2");
    import->set_working_directory("C:/work");

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
