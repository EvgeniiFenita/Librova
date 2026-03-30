#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <thread>

#include "Application/LibraryImportFacade.hpp"
#include "ApplicationJobs/ImportJobService.hpp"

namespace {

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
            .ImportedBookId = LibriFlow::Domain::SBookId{11}
        };
    }
};

class CBlockingSingleFileImporter final : public LibriFlow::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] LibriFlow::Importing::SSingleFileImportResult Run(
        const LibriFlow::Importing::SSingleFileImportRequest&,
        LibriFlow::Domain::IProgressSink& progressSink,
        std::stop_token stopToken) const override
    {
        progressSink.ReportValue(35, "Blocking import");

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
            .Status = LibriFlow::Importing::ESingleFileImportStatus::Cancelled,
            .Warnings = {"Cancelled by service test"}
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

} // namespace

TEST_CASE("Import job service starts and returns completed application-facing results", "[application-jobs]")
{
    CImmediateSingleFileImporter importer;
    LibriFlow::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    LibriFlow::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    LibriFlow::Jobs::CImportJobRunner runner(facade);
    LibriFlow::Jobs::CImportJobManager manager(runner);
    LibriFlow::ApplicationJobs::CImportJobService service(manager);

    const auto jobId = service.Start({
        .SourcePath = "C:/books/book.fb2",
        .WorkingDirectory = "C:/work"
    });

    REQUIRE(jobId != 0);
    REQUIRE(service.Wait(jobId, std::chrono::seconds(1)));

    const auto snapshot = service.TryGetSnapshot(jobId);
    const auto result = service.TryGetResult(jobId);

    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->JobId == jobId);
    REQUIRE(snapshot->Status == LibriFlow::ApplicationJobs::EImportJobStatus::Completed);
    REQUIRE(snapshot->IsTerminal());
    REQUIRE(result.has_value());
    REQUIRE(result->Snapshot.JobId == jobId);
    REQUIRE(result->ImportResult.has_value());
    REQUIRE(result->ImportResult->Summary.ImportedEntries == 1);
}

TEST_CASE("Import job service exposes cancellation through application-facing status", "[application-jobs]")
{
    CBlockingSingleFileImporter importer;
    LibriFlow::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    LibriFlow::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    LibriFlow::Jobs::CImportJobRunner runner(facade);
    LibriFlow::Jobs::CImportJobManager manager(runner);
    LibriFlow::ApplicationJobs::CImportJobService service(manager);

    const auto jobId = service.Start({
        .SourcePath = "C:/books/book.fb2",
        .WorkingDirectory = "C:/work"
    });

    REQUIRE(importer.WaitUntilStarted(std::chrono::seconds(1)));

    const auto snapshot = service.TryGetSnapshot(jobId);
    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->Status == LibriFlow::ApplicationJobs::EImportJobStatus::Running);
    REQUIRE(snapshot->Message == "Blocking import");

    REQUIRE(service.Cancel(jobId));
    REQUIRE(service.Wait(jobId, std::chrono::seconds(1)));

    const auto result = service.TryGetResult(jobId);
    REQUIRE(result.has_value());
    REQUIRE(result->Snapshot.Status == LibriFlow::ApplicationJobs::EImportJobStatus::Cancelled);
    REQUIRE(result->Error.has_value());
    REQUIRE(result->Error->Code == LibriFlow::Domain::EDomainErrorCode::Cancellation);
}

TEST_CASE("Import job service returns empty state for unknown jobs", "[application-jobs]")
{
    CImmediateSingleFileImporter importer;
    LibriFlow::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    LibriFlow::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    LibriFlow::Jobs::CImportJobRunner runner(facade);
    LibriFlow::Jobs::CImportJobManager manager(runner);
    LibriFlow::ApplicationJobs::CImportJobService service(manager);

    REQUIRE_FALSE(service.TryGetSnapshot(123).has_value());
    REQUIRE_FALSE(service.TryGetResult(123).has_value());
    REQUIRE_FALSE(service.Cancel(123));
    REQUIRE_FALSE(service.Wait(123, std::chrono::milliseconds(10)));
}
