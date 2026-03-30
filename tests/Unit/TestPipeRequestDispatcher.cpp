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
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service);
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
    Librova::Jobs::CImportJobRunner runner(facade);
    Librova::Jobs::CImportJobManager manager(runner);
    Librova::ApplicationJobs::CImportJobService service(manager);
    Librova::ProtoServices::CLibraryJobServiceAdapter adapter(service);
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
