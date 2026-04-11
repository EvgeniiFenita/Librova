#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <stop_token>
#include <thread>

#include "Application/LibraryImportFacade.hpp"
#include "Domain/BookRepository.hpp"
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

struct SImportSandbox
{
    std::filesystem::path Root;
    std::filesystem::path SourcePath;
    std::filesystem::path WorkingDirectory;
};

SImportSandbox CreateImportSandbox(const std::string_view scenario)
{
    const auto root = std::filesystem::temp_directory_path() / ("librova-job-manager-" + std::string{scenario});
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

TEST_CASE("Import job manager stores completed result for finished import", "[jobs][manager]")
{
    const auto sandbox = CreateImportSandbox("completed");
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        bookRepository,
        {.LibraryRoot = sandbox.Root});
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);

    const auto handle = manager.Start({
        .SourcePaths = {sandbox.SourcePath},
        .WorkingDirectory = sandbox.WorkingDirectory
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
    std::filesystem::remove_all(sandbox.Root);
}

TEST_CASE("Import job manager cancels a running job", "[jobs][manager]")
{
    const auto sandbox = CreateImportSandbox("cancel");
    CBlockingSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        bookRepository,
        {.LibraryRoot = sandbox.Root});
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);

    const auto handle = manager.Start({
        .SourcePaths = {sandbox.SourcePath},
        .WorkingDirectory = sandbox.WorkingDirectory
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
    std::filesystem::remove_all(sandbox.Root);
}

TEST_CASE("Import job manager returns empty results for unknown job id", "[jobs][manager]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        bookRepository,
        {.LibraryRoot = std::filesystem::temp_directory_path()});
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);

    REQUIRE_FALSE(manager.TryGetSnapshot(999).has_value());
    REQUIRE_FALSE(manager.TryGetResult(999).has_value());
    REQUIRE_FALSE(manager.Cancel(999));
    REQUIRE_FALSE(manager.Wait(999, std::chrono::milliseconds(10)));
}

TEST_CASE("Import job manager can remove completed jobs", "[jobs][manager]")
{
    const auto sandbox = CreateImportSandbox("remove");
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        bookRepository,
        {.LibraryRoot = sandbox.Root});
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);

    const auto handle = manager.Start({
        .SourcePaths = {sandbox.SourcePath},
        .WorkingDirectory = sandbox.WorkingDirectory
    });

    REQUIRE(manager.Wait(handle.Id, std::chrono::seconds(1)));
    REQUIRE(manager.Remove(handle.Id));
    REQUIRE_FALSE(manager.TryGetSnapshot(handle.Id).has_value());
    REQUIRE_FALSE(manager.TryGetResult(handle.Id).has_value());
    REQUIRE_FALSE(manager.Remove(handle.Id));
    std::filesystem::remove_all(sandbox.Root);
}

TEST_CASE("Import job manager does not remove running jobs", "[jobs][manager]")
{
    const auto sandbox = CreateImportSandbox("running-remove");
    CBlockingSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    CEmptyBookRepository bookRepository;
    Librova::Application::CLibraryImportFacade facade(
        importer,
        zipCoordinator,
        bookRepository,
        {.LibraryRoot = sandbox.Root});
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);

    const auto handle = manager.Start({
        .SourcePaths = {sandbox.SourcePath},
        .WorkingDirectory = sandbox.WorkingDirectory
    });

    REQUIRE(importer.WaitUntilStarted(std::chrono::seconds(1)));
    REQUIRE_FALSE(manager.Remove(handle.Id));
    REQUIRE(manager.Cancel(handle.Id));
    REQUIRE(manager.Wait(handle.Id, std::chrono::seconds(1)));
    std::filesystem::remove_all(sandbox.Root);
}
