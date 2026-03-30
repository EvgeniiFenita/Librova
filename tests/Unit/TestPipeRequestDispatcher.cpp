#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <thread>

#include "Application/LibraryCatalogFacade.hpp"
#include "Application/LibraryExportFacade.hpp"
#include "Application/LibraryImportFacade.hpp"
#include "ApplicationJobs/ImportJobService.hpp"
#include "BookDatabase/SqliteBookQueryRepository.hpp"
#include "BookDatabase/SqliteBookRepository.hpp"
#include "DatabaseRuntime/SchemaMigrator.hpp"
#include "Jobs/ImportJobManager.hpp"
#include "Jobs/ImportJobRunner.hpp"
#include "PipeTransport/PipeRequestDispatcher.hpp"

namespace {

class CImmediateSingleFileImporter final : public Librova::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] Librova::Importing::SSingleFileImportResult Run(
        const Librova::Importing::SSingleFileImportRequest&,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token) const override
    {
        progressSink.ReportValue(30, "Importing");
        return {
            .Status = Librova::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = Librova::Domain::SBookId{9}
        };
    }
};

} // namespace

TEST_CASE("Pipe dispatcher executes StartImport through protobuf adapter", "[pipe]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    class CEmptyQueryRepository final : public Librova::Domain::IBookQueryRepository
    {
    public:
        [[nodiscard]] std::vector<Librova::Domain::SBook> Search(const Librova::Domain::SSearchQuery&) const override
        {
            return {};
        }

        [[nodiscard]] std::vector<Librova::Domain::SDuplicateMatch> FindDuplicates(const Librova::Domain::SCandidateBook&) const override
        {
            return {};
        }
    } queryRepository;
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository);
    class CEmptyBookRepository final : public Librova::Domain::IBookRepository
    {
    public:
        [[nodiscard]] Librova::Domain::SBookId ReserveId() override { return {1}; }
        [[nodiscard]] Librova::Domain::SBookId Add(const Librova::Domain::SBook& book) override { return book.Id; }
        [[nodiscard]] std::optional<Librova::Domain::SBook> GetById(const Librova::Domain::SBookId) const override { return std::nullopt; }
        void Remove(const Librova::Domain::SBookId) override {}
    } bookRepository;
    Librova::Application::CLibraryExportFacade exportFacade(bookRepository, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, catalogFacade, exportFacade);
    Librova::PipeTransport::CPipeRequestDispatcher dispatcher(adapter);

    librova::v1::StartImportRequest typedRequest;
    auto* import = typedRequest.mutable_import();
    import->set_source_path("C:/books/book.fb2");
    import->set_working_directory("C:/work");

    std::string payload;
    REQUIRE(typedRequest.SerializeToString(&payload));

    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 1001,
        .Method = Librova::PipeTransport::EPipeMethod::StartImport,
        .Payload = payload
    };

    const auto response = dispatcher.Dispatch(request);
    REQUIRE(response.RequestId == request.RequestId);
    REQUIRE(response.Status == Librova::PipeTransport::EPipeResponseStatus::Ok);
    REQUIRE(response.ErrorMessage.empty());

    librova::v1::StartImportResponse typedResponse;
    REQUIRE(typedResponse.ParseFromString(response.Payload));
    REQUIRE(typedResponse.job_id() != 0);
}

TEST_CASE("Pipe dispatcher rejects invalid protobuf payloads", "[pipe]")
{
    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    class CEmptyQueryRepository final : public Librova::Domain::IBookQueryRepository
    {
    public:
        [[nodiscard]] std::vector<Librova::Domain::SBook> Search(const Librova::Domain::SSearchQuery&) const override
        {
            return {};
        }

        [[nodiscard]] std::vector<Librova::Domain::SDuplicateMatch> FindDuplicates(const Librova::Domain::SCandidateBook&) const override
        {
            return {};
        }
    } queryRepository;
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository);
    class CEmptyBookRepository final : public Librova::Domain::IBookRepository
    {
    public:
        [[nodiscard]] Librova::Domain::SBookId ReserveId() override { return {1}; }
        [[nodiscard]] Librova::Domain::SBookId Add(const Librova::Domain::SBook& book) override { return book.Id; }
        [[nodiscard]] std::optional<Librova::Domain::SBook> GetById(const Librova::Domain::SBookId) const override { return std::nullopt; }
        void Remove(const Librova::Domain::SBookId) override {}
    } bookRepository;
    Librova::Application::CLibraryExportFacade exportFacade(bookRepository, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, catalogFacade, exportFacade);
    Librova::PipeTransport::CPipeRequestDispatcher dispatcher(adapter);

    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 2002,
        .Method = Librova::PipeTransport::EPipeMethod::StartImport,
        .Payload = "not protobuf"
    };

    const auto response = dispatcher.Dispatch(request);
    REQUIRE(response.RequestId == request.RequestId);
    REQUIRE(response.Status == Librova::PipeTransport::EPipeResponseStatus::InvalidRequest);
    REQUIRE_FALSE(response.ErrorMessage.empty());
}

TEST_CASE("Pipe dispatcher executes ListBooks through protobuf adapter", "[pipe][catalog]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-pipe-dispatch-catalog.db";
    std::filesystem::remove(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    Librova::BookDatabase::CSqliteBookRepository writeRepository(databasePath);
    Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);

    Librova::Domain::SBook book;
    book.Metadata.TitleUtf8 = "Roadside Picnic";
    book.Metadata.AuthorsUtf8 = {"Arkady Strugatsky"};
    book.Metadata.Language = "en";
    book.File.ManagedPath = "Books/0000000301/book.epub";
    book.File.SizeBytes = 1024;
    book.File.Sha256Hex = "pipe-catalog-hash";
    book.AddedAtUtc = std::chrono::sys_days{std::chrono::March / 30 / 2026};
    static_cast<void>(writeRepository.Add(book));

    CImmediateSingleFileImporter importer;
    Librova::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    Librova::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, &writeRepository);
    Librova::Application::CLibraryExportFacade exportFacade(writeRepository, std::filesystem::temp_directory_path());
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service, catalogFacade, exportFacade);
    Librova::PipeTransport::CPipeRequestDispatcher dispatcher(adapter);

    librova::v1::ListBooksRequest typedRequest;
    typedRequest.mutable_query()->set_text("road");
    typedRequest.mutable_query()->set_limit(10);

    std::string payload;
    REQUIRE(typedRequest.SerializeToString(&payload));

    const Librova::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 3003,
        .Method = Librova::PipeTransport::EPipeMethod::ListBooks,
        .Payload = payload
    };

    const auto response = dispatcher.Dispatch(request);
    REQUIRE(response.RequestId == request.RequestId);
    REQUIRE(response.Status == Librova::PipeTransport::EPipeResponseStatus::Ok);

    librova::v1::ListBooksResponse typedResponse;
    REQUIRE(typedResponse.ParseFromString(response.Payload));
    REQUIRE(typedResponse.items_size() == 1);
    REQUIRE(typedResponse.items(0).title() == "Roadside Picnic");

    std::filesystem::remove(databasePath);
}
