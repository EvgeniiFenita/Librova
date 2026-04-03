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
#include "ApplicationClient/ImportJobClient.hpp"
#include "ApplicationJobs/ImportJobService.hpp"
#include "Jobs/ImportJobManager.hpp"
#include "Jobs/ImportJobRunner.hpp"
#include "ManagedTrash/ManagedTrashService.hpp"
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

class CEmptyQueryRepository final : public Librova::Domain::IBookQueryRepository
{
public:
    [[nodiscard]] std::vector<Librova::Domain::SBook> Search(const Librova::Domain::SSearchQuery&) const override
    {
        return {};
    }

    [[nodiscard]] std::uint64_t CountSearchResults(const Librova::Domain::SSearchQuery&) const override
    {
        return 0;
    }

    [[nodiscard]] std::vector<std::string> ListAvailableLanguages(const Librova::Domain::SSearchQuery&) const override
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

struct SImportSandbox
{
    std::filesystem::path Root;
    std::filesystem::path SourcePath;
    std::filesystem::path WorkingDirectory;
};

SImportSandbox CreateImportSandbox()
{
    const auto root = std::filesystem::temp_directory_path() / "librova-app-client-import";
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

TEST_CASE("Application import job client performs end-to-end start wait and result retrieval", "[application-client]")
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
    const auto sandbox = CreateImportSandbox();

    const auto jobId = client.Start({
        .SourcePaths = {sandbox.SourcePath},
        .WorkingDirectory = sandbox.WorkingDirectory
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
    std::filesystem::remove_all(sandbox.Root);
}
