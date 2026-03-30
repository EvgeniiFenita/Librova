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

class CImmediateSingleFileImporter final : public LibriFlow::Importing::ISingleFileImporter
{
public:
    [[nodiscard]] LibriFlow::Importing::SSingleFileImportResult Run(
        const LibriFlow::Importing::SSingleFileImportRequest&,
        LibriFlow::Domain::IProgressSink& progressSink,
        std::stop_token) const override
    {
        progressSink.ReportValue(30, "Importing");
        return {
            .Status = LibriFlow::Importing::ESingleFileImportStatus::Imported,
            .ImportedBookId = LibriFlow::Domain::SBookId{9}
        };
    }
};

} // namespace

TEST_CASE("Pipe dispatcher executes StartImport through protobuf adapter", "[pipe]")
{
    CImmediateSingleFileImporter importer;
    LibriFlow::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    LibriFlow::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    LibriFlow::Jobs::CImportJobRunner runner(facade);
    LibriFlow::Jobs::CImportJobManager manager(runner);
    LibriFlow::ApplicationJobs::CImportJobService service(manager);
    LibriFlow::ProtoServices::CLibraryJobServiceAdapter adapter(service);
    LibriFlow::PipeTransport::CPipeRequestDispatcher dispatcher(adapter);

    libriflow::v1::StartImportRequest typedRequest;
    auto* import = typedRequest.mutable_import();
    import->set_source_path("C:/books/book.fb2");
    import->set_working_directory("C:/work");

    std::string payload;
    REQUIRE(typedRequest.SerializeToString(&payload));

    const LibriFlow::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 1001,
        .Method = LibriFlow::PipeTransport::EPipeMethod::StartImport,
        .Payload = payload
    };

    const auto response = dispatcher.Dispatch(request);
    REQUIRE(response.RequestId == request.RequestId);
    REQUIRE(response.Status == LibriFlow::PipeTransport::EPipeResponseStatus::Ok);
    REQUIRE(response.ErrorMessage.empty());

    libriflow::v1::StartImportResponse typedResponse;
    REQUIRE(typedResponse.ParseFromString(response.Payload));
    REQUIRE(typedResponse.job_id() != 0);
}

TEST_CASE("Pipe dispatcher rejects invalid protobuf payloads", "[pipe]")
{
    CImmediateSingleFileImporter importer;
    LibriFlow::ZipImporting::CZipImportCoordinator zipCoordinator(importer);
    LibriFlow::Application::CLibraryImportFacade facade(importer, zipCoordinator);
    LibriFlow::Jobs::CImportJobRunner runner(facade);
    LibriFlow::Jobs::CImportJobManager manager(runner);
    LibriFlow::ApplicationJobs::CImportJobService service(manager);
    LibriFlow::ProtoServices::CLibraryJobServiceAdapter adapter(service);
    LibriFlow::PipeTransport::CPipeRequestDispatcher dispatcher(adapter);

    const LibriFlow::PipeTransport::SPipeRequestEnvelope request{
        .RequestId = 2002,
        .Method = LibriFlow::PipeTransport::EPipeMethod::StartImport,
        .Payload = "not protobuf"
    };

    const auto response = dispatcher.Dispatch(request);
    REQUIRE(response.RequestId == request.RequestId);
    REQUIRE(response.Status == LibriFlow::PipeTransport::EPipeResponseStatus::InvalidRequest);
    REQUIRE_FALSE(response.ErrorMessage.empty());
}
