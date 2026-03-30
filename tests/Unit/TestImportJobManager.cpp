#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <thread>

#include "Application/LibraryImportFacade.hpp"
#include "Jobs/ImportJobManager.hpp"

namespace {

class CImmediateSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest&,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token) const override
    {
        progressSink.ReportValue(30, "Parsing");
        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{7}
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
        progressSink.ReportValue(25, "Waiting inside importer");

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
            .Warnings = {"Cancelled by test"}
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

TEST_CASE("Import job manager stores completed result for finished import", "[jobs][manager]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);

    const auto handle = manager.Start({
        .SourcePath = "C:/books/book.fb2",
        .WorkingDirectory = "C:/work"
    });

    REQUIRE(handle.IsValid());
    REQUIRE(manager.Wait(handle.Id, std::chrono::seconds(1)));

    const auto snapshot = manager.TryGetSnapshot(handle.Id);
    const auto result = manager.TryGetResult(handle.Id);

    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->Status == Librova::Jobs::EJobStatus::Completed);
    REQUIRE(snapshot->IsTerminal());
    REQUIRE(result.has_value());
    REQUIRE(result->ImportResult.has_value());
    REQUIRE(result->ImportResult->Summary.ImportedEntries == 1);
}

TEST_CASE("Import job manager cancels a running job", "[jobs][manager]")
{
    CBlockingSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);

    const auto handle = manager.Start({
        .SourcePath = "C:/books/book.fb2",
        .WorkingDirectory = "C:/work"
    });

    REQUIRE(handle.IsValid());
    REQUIRE(importer.WaitUntilStarted(std::chrono::seconds(1)));

    const auto midSnapshot = manager.TryGetSnapshot(handle.Id);
    REQUIRE(midSnapshot.has_value());
    REQUIRE(midSnapshot->Status == Librova::Jobs::EJobStatus::Running);
    REQUIRE(midSnapshot->Message == "Waiting inside importer");

    REQUIRE(manager.Cancel(handle.Id));
    REQUIRE(manager.Wait(handle.Id, std::chrono::seconds(1)));

    const auto result = manager.TryGetResult(handle.Id);
    REQUIRE(result.has_value());
    REQUIRE(result->Snapshot.Status == Librova::Jobs::EJobStatus::Cancelled);
    REQUIRE(result->Error.has_value());
    REQUIRE(result->Error->Code == Librova::Domain::EDomainErrorCode::Cancellation);
}

TEST_CASE("Import job manager returns empty results for unknown job id", "[jobs][manager]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);

    REQUIRE_FALSE(manager.TryGetSnapshot(999).has_value());
    REQUIRE_FALSE(manager.TryGetResult(999).has_value());
    REQUIRE_FALSE(manager.Cancel(999));
    REQUIRE_FALSE(manager.Wait(999, std::chrono::milliseconds(10)));
}

TEST_CASE("Import job manager can remove completed jobs", "[jobs][manager]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);

    const auto handle = manager.Start({
        .SourcePath = "C:/books/book.fb2",
        .WorkingDirectory = "C:/work"
    });

    REQUIRE(manager.Wait(handle.Id, std::chrono::seconds(1)));
    REQUIRE(manager.Remove(handle.Id));
    REQUIRE_FALSE(manager.TryGetSnapshot(handle.Id).has_value());
    REQUIRE_FALSE(manager.TryGetResult(handle.Id).has_value());
    REQUIRE_FALSE(manager.Remove(handle.Id));
}

TEST_CASE("Import job manager does not remove running jobs", "[jobs][manager]")
{
    CBlockingSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);

    const auto handle = manager.Start({
        .SourcePath = "C:/books/book.fb2",
        .WorkingDirectory = "C:/work"
    });

    REQUIRE(importer.WaitUntilStarted(std::chrono::seconds(1)));
    REQUIRE_FALSE(manager.Remove(handle.Id));
    REQUIRE(manager.Cancel(handle.Id));
    REQUIRE(manager.Wait(handle.Id, std::chrono::seconds(1)));
}
