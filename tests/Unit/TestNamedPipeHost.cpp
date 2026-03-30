#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <condition_variable>
#include <exception>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>

#include "Application/LibraryImportFacade.hpp"
#include "ApplicationJobs/ImportJobService.hpp"
#include "Jobs/ImportJobManager.hpp"
#include "Jobs/ImportJobRunner.hpp"
#include "PipeHost/NamedPipeHost.hpp"

namespace {

std::filesystem::path BuildTestPipePath()
{
    const auto uniqueId = std::to_wstring(
        static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
    return std::filesystem::path{std::wstring{LR"(\\.\pipe\LibriFlow.Host.Test.)"} + uniqueId};
}

class CImmediateSingleFileImporter final : public LibriFlow::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] LibriFlow::Importing::SSingleFileImportResult Run(
        const LibriFlow::Importing::SSingleFileImportRequest&,
        LibriFlow::Domain::IProgressSink& progressSink,
        std::stop_token) const override
    {
        progressSink.ReportValue(20, "Parsing");
        return {
            .Status = LibriFlow::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = LibriFlow::Domain::SBookId{17}
        };
    }
};

} // namespace

TEST_CASE("Named pipe host serves a protobuf StartImport request end-to-end", "[pipe-host]")
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

    auto client = LibriFlow::PipeTransport::ConnectToNamedPipe(pipePath, std::chrono::seconds(2));

    libriflow::v1::StartImportRequest typedRequest;
    auto* import = typedRequest.mutable_import();
    import->set_source_path("C:/books/book.fb2");
    import->set_working_directory("C:/work");

    std::string payload;
    REQUIRE(typedRequest.SerializeToString(&payload));

    const LibriFlow::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 5001,
        .Method = LibriFlow::PipeTransport::EPipeMethod::StartImport,
        .Payload = std::move(payload)
    };

    client.WriteMessage(LibriFlow::PipeTransport::SerializeRequestEnvelope(request));

    const auto responseBytes = client.ReadMessage();
    const auto parsedResponse = LibriFlow::PipeTransport::DeserializeResponseEnvelope(responseBytes);
    REQUIRE(parsedResponse.HasValue());
    REQUIRE(parsedResponse.Value->RequestId == request.RequestId);
    REQUIRE(parsedResponse.Value->Status == LibriFlow::PipeTransport::EPipeResponseStatus::Ok);

    libriflow::v1::StartImportResponse typedResponse;
    REQUIRE(typedResponse.ParseFromString(parsedResponse.Value->Payload));
    REQUIRE(typedResponse.job_id() != 0);

    serverThread.join();
    REQUIRE(serverFailure == nullptr);
}
