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
    return std::filesystem::path{std::wstring{LR"(\\.\pipe\LibriFlow.Client.Test.)"} + uniqueId};
}

class CImmediateSingleFileImporter final : public LibriFlow::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] LibriFlow::Importing::SSingleFileImportResult Run(
        const LibriFlow::Importing::SSingleFileImportRequest&,
        LibriFlow::Domain::IProgressSink& progressSink,
        std::stop_token) const override
    {
        progressSink.ReportValue(10, "Parsing");
        return {
            .Status = LibriFlow::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = LibriFlow::Domain::SBookId{23}
        };
    }
};

} // namespace

TEST_CASE("Named pipe client performs typed StartImport call through host", "[pipe-client]")
{
    CImmediateSingleFileImporter importer;
    LibriFlow::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    LibriFlow::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    LibriFlow::Jobs::CImportJobRunner runner(facade);
    LibriFlow::Jobs::CImportJobManager manager(runner);
    LibriFlow::ApplicationJobs::CImportJobService service(manager);
    LibriFlow::ProtoServices::CLibraryJobServiceAdapter adapter(service);
    LibriFlow::PipeTransport::CPipeRequestDispatcher dispatcher(adapter);
    LibriFlow::PipeHost::CNamedPipeHost host(dispatcher);

    const auto pipePath = BuildTestPipePath();
    std::exception_ptr serverFailure;

    std::jthread serverThread([&] {
        try
        {
            LibriFlow::PipeTransport::CNamedPipeServer server(pipePath);
            host.RunSingleSession(server.WaitForClient());
        }
        catch (...)
        {
            serverFailure = std::current_exception();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    LibriFlow::PipeClient::CNamedPipeClient client(pipePath);

    libriflow::v1::StartImportRequest request;
    auto* import = request.mutable_import();
    import->set_source_path("C:/books/book.fb2");
    import->set_working_directory("C:/work");

    const auto response = client.Call<libriflow::v1::StartImportRequest, libriflow::v1::StartImportResponse>(
        LibriFlow::PipeTransport::EPipeMethod::StartImport,
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
            LibriFlow::PipeTransport::CNamedPipeServer server(pipePath);
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

    LibriFlow::PipeClient::CNamedPipeClient client(pipePath);

    libriflow::v1::WaitImportJobRequest request;
    request.set_job_id(1);
    request.set_timeout_ms(1);

    const auto failingCall = [&client, &request] {
        return client.Call<libriflow::v1::WaitImportJobRequest, libriflow::v1::WaitImportJobResponse>(
            LibriFlow::PipeTransport::EPipeMethod::WaitImportJob,
            request,
            std::chrono::seconds(2));
    };

    REQUIRE_THROWS_AS(failingCall(), LibriFlow::PipeClient::CPipeTransportError);

    serverThread.join();
    REQUIRE(serverFailure == nullptr);
}
