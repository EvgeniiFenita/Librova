#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <thread>

#include "Application/LibraryImportFacade.hpp"
#include "ApplicationJobs/ImportJobService.hpp"
#include "Jobs/ImportJobManager.hpp"
#include "Jobs/ImportJobRunner.hpp"
#include "ProtoServices/LibraryJobServiceAdapter.hpp"

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
            .ImportedBookId = LibriFlow::Domain::SBookId{12}
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
            .Status = LibriFlow::Importing::ESingleFileImportStatus::Cancelled,
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

} // namespace

TEST_CASE("Library job service adapter starts jobs and returns protobuf results", "[proto-service]")
{
    CImmediateSingleFileImporter importer;
    LibriFlow::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    LibriFlow::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    LibriFlow::Jobs::CImportJobRunner runner(facade);
    LibriFlow::Jobs::CImportJobManager manager(runner);
    LibriFlow::ApplicationJobs::CImportJobService service(manager);
    LibriFlow::ProtoServices::CLibraryJobServiceAdapter adapter(service);

    libriflow::v1::StartImportRequest startRequest;
    auto* import = startRequest.mutable_import();
    import->set_source_path("C:/books/book.fb2");
    import->set_working_directory("C:/work");
    import->set_allow_probable_duplicates(true);

    const auto startResponse = adapter.StartImport(startRequest);
    REQUIRE(startResponse.job_id() != 0);

    libriflow::v1::WaitImportJobRequest waitRequest;
    waitRequest.set_job_id(startResponse.job_id());
    waitRequest.set_timeout_ms(1000);

    const auto waitResponse = adapter.WaitImportJob(waitRequest);
    REQUIRE(waitResponse.completed());

    libriflow::v1::GetImportJobResultRequest resultRequest;
    resultRequest.set_job_id(startResponse.job_id());

    const auto resultResponse = adapter.GetImportJobResult(resultRequest);
    REQUIRE(resultResponse.has_result());
    REQUIRE(resultResponse.result().snapshot().job_id() == startResponse.job_id());
    REQUIRE(resultResponse.result().summary().imported_entries() == 1);
}

TEST_CASE("Library job service adapter exposes snapshot cancellation and removal", "[proto-service]")
{
    CBlockingSingleFileImporter importer;
    LibriFlow::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    LibriFlow::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    LibriFlow::Jobs::CImportJobRunner runner(facade);
    LibriFlow::Jobs::CImportJobManager manager(runner);
    LibriFlow::ApplicationJobs::CImportJobService service(manager);
    LibriFlow::ProtoServices::CLibraryJobServiceAdapter adapter(service);

    libriflow::v1::StartImportRequest startRequest;
    startRequest.mutable_import()->set_source_path("C:/books/book.fb2");
    startRequest.mutable_import()->set_working_directory("C:/work");

    const auto startResponse = adapter.StartImport(startRequest);
    REQUIRE(importer.WaitUntilStarted(std::chrono::seconds(1)));

    libriflow::v1::GetImportJobSnapshotRequest snapshotRequest;
    snapshotRequest.set_job_id(startResponse.job_id());

    const auto snapshotResponse = adapter.GetImportJobSnapshot(snapshotRequest);
    REQUIRE(snapshotResponse.has_snapshot());
    REQUIRE(snapshotResponse.snapshot().status() == libriflow::v1::IMPORT_JOB_STATUS_RUNNING);

    libriflow::v1::CancelImportJobRequest cancelRequest;
    cancelRequest.set_job_id(startResponse.job_id());
    REQUIRE(adapter.CancelImportJob(cancelRequest).accepted());

    libriflow::v1::WaitImportJobRequest waitRequest;
    waitRequest.set_job_id(startResponse.job_id());
    waitRequest.set_timeout_ms(1000);
    REQUIRE(adapter.WaitImportJob(waitRequest).completed());

    libriflow::v1::RemoveImportJobRequest removeRequest;
    removeRequest.set_job_id(startResponse.job_id());
    REQUIRE(adapter.RemoveImportJob(removeRequest).removed());
    REQUIRE_FALSE(adapter.GetImportJobSnapshot(snapshotRequest).has_snapshot());
}
