#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <exception>
#include <filesystem>
#include <stop_token>
#include <string>
#include <thread>

#include "Application/LibraryImportFacade.hpp"
#include "ApplicationClient/ImportJobClient.hpp"
#include "ApplicationJobs/ImportJobService.hpp"
#include "Jobs/ImportJobManager.hpp"
#include "Jobs/ImportJobRunner.hpp"
#include "PipeHost/NamedPipeHost.hpp"

namespace {

std::filesystem::path BuildTestPipePath()
{
    const auto uniqueId = std::to_wstring(
        static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
    return std::filesystem::path{std::wstring{LR"(\\.\pipe\Librova.AppClient.Test.)"} + uniqueId};
}

class CImmediateSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest&,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token) const override
    {
        progressSink.ReportValue(15, "Parsing");
        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{31}
        };
    }
};

} // namespace

TEST_CASE("Application import job client performs end-to-end start wait and result retrieval", "[application-client]")
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
            for (int index = 0; index < 3; ++index)
            {
                Librova::PipeTransport::CNamedPipeServer server(pipePath);
                host.RunSingleSession(server.WaitForClient());
            }
        }
        catch (...)
        {
            serverFailure = std::current_exception();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    Librova::ApplicationClient::CImportJobClient client(pipePath);

    const auto jobId = client.Start({
        .SourcePath = "C:/books/book.fb2",
        .WorkingDirectory = "C:/work"
    }, std::chrono::seconds(2));

    REQUIRE(jobId != 0);
    REQUIRE(client.Wait(jobId, std::chrono::seconds(2), std::chrono::seconds(1)));

    const auto result = client.TryGetResult(jobId, std::chrono::seconds(2));
    REQUIRE(result.has_value());
    REQUIRE(result->Snapshot.JobId == jobId);
    REQUIRE(result->Snapshot.Status == Librova::ApplicationJobs::EImportJobStatus::Completed);
    REQUIRE(result->ImportResult.has_value());
    REQUIRE(result->ImportResult->Summary.ImportedEntries == 1);

    serverThread.join();
    REQUIRE(serverFailure == nullptr);
}
