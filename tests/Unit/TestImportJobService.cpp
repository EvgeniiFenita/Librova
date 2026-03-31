#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <stop_token>
#include <thread>

#include "Application/LibraryImportFacade.hpp"
#include "ApplicationJobs/ImportJobService.hpp"

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
            .ImportedBookId = Librova::Domain::SBookId{11}
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
            .Status = Librova::Importing::ESingleFileImportStatus::Cancelled,
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

struct SImportSandbox
{
    std::filesystem::path Root;
    std::filesystem::path SourcePath;
    std::filesystem::path WorkingDirectory;
};

SImportSandbox CreateImportSandbox(const std::string_view scenario)
{
    const auto root = std::filesystem::temp_directory_path() / ("librova-job-service-" + std::string{scenario});
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

TEST_CASE("Import job service starts and returns completed application-facing results", "[application-jobs]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    const auto sandbox = CreateImportSandbox("completed");

    const auto jobId = service.Start({
        .SourcePaths = {sandbox.SourcePath},
        .WorkingDirectory = sandbox.WorkingDirectory
    });

    REQUIRE(jobId != 0);
    REQUIRE(service.Wait(jobId, std::chrono::seconds(1)));

    const auto snapshot = service.TryGetSnapshot(jobId);
    const auto result = service.TryGetResult(jobId);

    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->JobId == jobId);
    REQUIRE(snapshot->Status == Librova::ApplicationJobs::EImportJobStatus::Completed);
    REQUIRE(snapshot->IsTerminal());
    REQUIRE(result.has_value());
    REQUIRE(result->Snapshot.JobId == jobId);
    REQUIRE(result->ImportResult.has_value());
    REQUIRE(result->ImportResult->Summary.ImportedEntries == 1);
    std::filesystem::remove_all(sandbox.Root);
}

TEST_CASE("Import job service exposes cancellation through application-facing status", "[application-jobs]")
{
    CBlockingSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    const auto sandbox = CreateImportSandbox("cancel");

    const auto jobId = service.Start({
        .SourcePaths = {sandbox.SourcePath},
        .WorkingDirectory = sandbox.WorkingDirectory
    });

    REQUIRE(importer.WaitUntilStarted(std::chrono::seconds(1)));

    const auto snapshot = service.TryGetSnapshot(jobId);
    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->Status == Librova::ApplicationJobs::EImportJobStatus::Running);
    REQUIRE(snapshot->Message == "Blocking import");

    REQUIRE(service.Cancel(jobId));
    REQUIRE(service.Wait(jobId, std::chrono::seconds(1)));

    const auto result = service.TryGetResult(jobId);
    REQUIRE(result.has_value());
    REQUIRE(result->Snapshot.Status == Librova::ApplicationJobs::EImportJobStatus::Cancelled);
    REQUIRE(result->Error.has_value());
    REQUIRE(result->Error->Code == Librova::Domain::EDomainErrorCode::Cancellation);
    std::filesystem::remove_all(sandbox.Root);
}

TEST_CASE("Import job service returns empty state for unknown jobs", "[application-jobs]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);

    REQUIRE_FALSE(service.TryGetSnapshot(123).has_value());
    REQUIRE_FALSE(service.TryGetResult(123).has_value());
    REQUIRE_FALSE(service.Cancel(123));
    REQUIRE_FALSE(service.Wait(123, std::chrono::milliseconds(10)));
}

TEST_CASE("Import job service removes completed jobs through application-facing API", "[application-jobs]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    const auto sandbox = CreateImportSandbox("remove");

    const auto jobId = service.Start({
        .SourcePaths = {sandbox.SourcePath},
        .WorkingDirectory = sandbox.WorkingDirectory
    });

    REQUIRE(service.Wait(jobId, std::chrono::seconds(1)));
    REQUIRE(service.Remove(jobId));
    REQUIRE_FALSE(service.TryGetSnapshot(jobId).has_value());
    REQUIRE_FALSE(service.TryGetResult(jobId).has_value());
    std::filesystem::remove_all(sandbox.Root);
}
