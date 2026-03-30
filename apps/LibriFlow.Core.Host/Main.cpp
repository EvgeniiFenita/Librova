#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Application/LibraryImportFacade.hpp"
#include "ApplicationJobs/ImportJobService.hpp"
#include "BookDatabase/SqliteBookQueryRepository.hpp"
#include "BookDatabase/SqliteBookRepository.hpp"
#include "ConverterConfiguration/ConverterConfiguration.hpp"
#include "ConverterRuntime/ExternalBookConverter.hpp"
#include "CoreHost/HostOptions.hpp"
#include "CoreHost/LibraryBootstrap.hpp"
#include "DatabaseRuntime/SchemaMigrator.hpp"
#include "Importing/SingleFileImportCoordinator.hpp"
#include "Jobs/ImportJobManager.hpp"
#include "Jobs/ImportJobRunner.hpp"
#include "ManagedStorage/ManagedFileStorage.hpp"
#include "ParserRegistry/BookParserRegistry.hpp"
#include "PipeHost/NamedPipeHost.hpp"
#include "PipeTransport/PipeRequestDispatcher.hpp"
#include "ProtoServices/LibraryJobServiceAdapter.hpp"
#include "StoragePlanning/ManagedLibraryLayout.hpp"
#include "ZipImporting/ZipImportCoordinator.hpp"

namespace {

[[nodiscard]] std::vector<std::string> CollectArguments(const int argc, char** argv)
{
    std::vector<std::string> arguments;
    arguments.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0);

    for (int index = 1; index < argc; ++index)
    {
        arguments.emplace_back(argv[index]);
    }

    return arguments;
}

[[nodiscard]] std::filesystem::path GetDatabasePath(const std::filesystem::path& libraryRoot)
{
    return LibriFlow::StoragePlanning::CManagedLibraryLayout::Build(libraryRoot).DatabaseDirectory / "libriflow.db";
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const auto options = LibriFlow::CoreHost::CHostOptions::Parse(CollectArguments(argc, argv));

        LibriFlow::CoreHost::CLibraryBootstrap::PrepareLibraryRoot(options.LibraryRoot);

        const auto databasePath = GetDatabasePath(options.LibraryRoot);
        LibriFlow::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

        const LibriFlow::ParserRegistry::CBookParserRegistry parserRegistry;
        LibriFlow::BookDatabase::CSqliteBookRepository bookRepository(databasePath);
        LibriFlow::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);
        LibriFlow::ManagedStorage::CManagedFileStorage managedStorage(options.LibraryRoot);

        std::optional<LibriFlow::ConverterRuntime::CExternalBookConverter> converter;
        if (const auto profile =
                LibriFlow::ConverterConfiguration::TryBuildCommandProfile(options.ConverterConfiguration);
            profile.has_value())
        {
            converter.emplace(LibriFlow::ConverterRuntime::SExternalConverterSettings{
                .CommandProfile = *profile
            });
        }

        const auto* converterPtr =
            converter.has_value() ? static_cast<const LibriFlow::Domain::IBookConverter*>(&*converter) : nullptr;

        const LibriFlow::Importing::CSingleFileImportCoordinator singleFileImporter(
            parserRegistry,
            bookRepository,
            queryRepository,
            managedStorage,
            converterPtr);
        const LibriFlow::ZipImporting::CZipImportCoordinator zipImportCoordinator(singleFileImporter);
        const LibriFlow::Application::CLibraryImportFacade importFacade(singleFileImporter, zipImportCoordinator);
        const LibriFlow::Jobs::CImportJobRunner jobRunner(importFacade);
        LibriFlow::Jobs::CImportJobManager jobManager(jobRunner);
        LibriFlow::ApplicationJobs::CImportJobService jobService(jobManager);
        LibriFlow::ProtoServices::CLibraryJobServiceAdapter serviceAdapter(jobService);
        LibriFlow::PipeTransport::CPipeRequestDispatcher dispatcher(serviceAdapter);
        const LibriFlow::PipeHost::CNamedPipeHost host(dispatcher);

        std::size_t servedSessions = 0;
        while (options.MaxSessions == 0 || servedSessions < options.MaxSessions)
        {
            try
            {
                LibriFlow::PipeTransport::CNamedPipeServer server(options.PipePath);
                host.RunSingleSession(server.WaitForClient());
                ++servedSessions;
            }
            catch (const std::exception& error)
            {
                std::cerr << "LibriFlow.Core.Host session error: " << error.what() << '\n';
            }
        }

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "LibriFlow.Core.Host failed: " << error.what() << '\n';
        return 1;
    }
}
