#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Application/LibraryImportFacade.hpp"
#include "Application/LibraryCatalogFacade.hpp"
#include "Application/LibraryExportFacade.hpp"
#include "Application/LibraryTrashFacade.hpp"
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
#include "Logging/Logging.hpp"
#include "ManagedStorage/ManagedFileStorage.hpp"
#include "ManagedTrash/ManagedTrashService.hpp"
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
    return Librova::StoragePlanning::CManagedLibraryLayout::Build(libraryRoot).DatabaseDirectory / "librova.db";
}

[[nodiscard]] std::filesystem::path GetLogFilePath(const std::filesystem::path& libraryRoot)
{
    return Librova::StoragePlanning::CManagedLibraryLayout::Build(libraryRoot).LogsDirectory / "host.log";
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const auto options = Librova::CoreHost::CHostOptions::Parse(CollectArguments(argc, argv));

        Librova::CoreHost::CLibraryBootstrap::PrepareLibraryRoot(options.LibraryRoot);
        Librova::Logging::CLogging::InitializeHostLogger(GetLogFilePath(options.LibraryRoot));
        Librova::Logging::Info("Starting Librova.Core.Host for library root '{}'.", options.LibraryRoot.string());

        const auto databasePath = GetDatabasePath(options.LibraryRoot);
        Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

        const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
        Librova::BookDatabase::CSqliteBookRepository bookRepository(databasePath);
        Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);
        Librova::ManagedStorage::CManagedFileStorage managedStorage(options.LibraryRoot);
        Librova::ManagedTrash::CManagedTrashService trashService(options.LibraryRoot);

        std::optional<Librova::ConverterRuntime::CExternalBookConverter> converter;
        if (const auto profile =
                Librova::ConverterConfiguration::TryBuildCommandProfile(options.ConverterConfiguration);
            profile.has_value())
        {
            converter.emplace(Librova::ConverterRuntime::SExternalConverterSettings{
                .CommandProfile = *profile
            });
        }

        const auto* converterPtr =
            converter.has_value() ? static_cast<const Librova::Domain::IBookConverter*>(&*converter) : nullptr;

        const Librova::Importing::CSingleFileImportCoordinator singleFileImporter(
            parserRegistry,
            bookRepository,
            queryRepository,
            managedStorage,
            converterPtr);
        const Librova::ZipImporting::CZipImportCoordinator zipImportCoordinator(singleFileImporter);
        const Librova::Application::CLibraryImportFacade importFacade(singleFileImporter, zipImportCoordinator);
        const Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, &bookRepository);
        const Librova::Application::CLibraryExportFacade exportFacade(bookRepository, options.LibraryRoot);
        const Librova::Application::CLibraryTrashFacade trashFacade(bookRepository, trashService, options.LibraryRoot);
        const Librova::Jobs::CImportJobRunner jobRunner(importFacade);
        Librova::Jobs::CImportJobManager jobManager(jobRunner);
        Librova::ApplicationJobs::CImportJobService jobService(jobManager);
        Librova::ProtoServices::CLibraryJobServiceAdapter serviceAdapter(jobService, catalogFacade, exportFacade, trashFacade);
        Librova::PipeTransport::CPipeRequestDispatcher dispatcher(serviceAdapter);
        const Librova::PipeHost::CNamedPipeHost host(dispatcher);

        std::size_t servedSessions = 0;
        while (options.MaxSessions == 0 || servedSessions < options.MaxSessions)
        {
            try
            {
                Librova::PipeTransport::CNamedPipeServer server(options.PipePath);
                Librova::Logging::Info("Waiting for pipe session on '{}'.", options.PipePath.string());
                host.RunSingleSession(server.WaitForClient());
                Librova::Logging::Info("Pipe session completed successfully.");
                ++servedSessions;
            }
            catch (const std::exception& error)
            {
                Librova::Logging::Error("Pipe session failed: {}", error.what());
                std::cerr << "Librova.Core.Host session error: " << error.what() << '\n';
            }
        }

        Librova::Logging::Info("Librova.Core.Host stopped after serving {} sessions.", servedSessions);
        Librova::Logging::CLogging::Shutdown();
        return 0;
    }
    catch (const std::exception& error)
    {
        if (Librova::Logging::CLogging::IsInitialized())
        {
            Librova::Logging::Critical("Librova.Core.Host failed: {}", error.what());
            Librova::Logging::CLogging::Shutdown();
        }

        std::cerr << "Librova.Core.Host failed: " << error.what() << '\n';
        return 1;
    }
}
