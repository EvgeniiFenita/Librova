#include <chrono>
#include <atomic>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
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
#include "CoverProcessingStb/StbCoverImageProcessor.hpp"
#include "Core/Version.hpp"
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
#include "PipeTransport/NamedPipeChannel.hpp"
#include "PipeTransport/PipeRequestDispatcher.hpp"
#include "ProtoServices/LibraryJobServiceAdapter.hpp"
#include "RecycleBin/WindowsRecycleBinService.hpp"
#include "StoragePlanning/ManagedLibraryLayout.hpp"
#include "Unicode/UnicodeConversion.hpp"
#include "ZipImporting/ZipImportCoordinator.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#endif

namespace {

[[nodiscard]] std::vector<std::string> CollectArguments(const int argc, char** argv)
{
#ifdef _WIN32
    int wideArgc = 0;
    LPWSTR* wideArgv = ::CommandLineToArgvW(::GetCommandLineW(), &wideArgc);
    if (wideArgv == nullptr)
    {
        throw std::runtime_error("Failed to read Unicode command-line arguments.");
    }

    std::vector<std::string> arguments;
    arguments.reserve(wideArgc > 1 ? static_cast<std::size_t>(wideArgc - 1) : 0);

    try
    {
        for (int index = 1; index < wideArgc; ++index)
        {
            arguments.push_back(Librova::Unicode::WideToUtf8(wideArgv[index]));
        }
    }
    catch (...)
    {
        ::LocalFree(wideArgv);
        throw;
    }

    ::LocalFree(wideArgv);
    return arguments;
#else
    std::vector<std::string> arguments;
    arguments.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0);

    for (int index = 1; index < argc; ++index)
    {
        arguments.emplace_back(argv[index]);
    }

    return arguments;
#endif
}

[[nodiscard]] std::filesystem::path GetDatabasePath(const std::filesystem::path& libraryRoot)
{
    return Librova::StoragePlanning::CManagedLibraryLayout::Build(libraryRoot).DatabaseDirectory / "librova.db";
}

[[nodiscard]] std::filesystem::path GetLogFilePath(const Librova::CoreHost::SHostOptions& options)
{
    if (options.LogFilePath.has_value())
    {
        return *options.LogFilePath;
    }

    return Librova::StoragePlanning::CManagedLibraryLayout::Build(options.LibraryRoot).LogsDirectory / "host.log";
}

void PrintUsage()
{
    std::cout
        << "Usage: Librova.Core.Host --pipe <path> --library-root <path> [options]\n"
        << "Options:\n"
        << "  --help, -h              Show this help and exit.\n"
        << "  --version               Show host version and exit.\n"
        << "  --library-mode <open|create>\n"
        << "                         Open an existing library or create a new one.\n"
        << "  --parent-pid <pid>      Bind host lifetime to the given parent process.\n"
        << "  --serve-one             Stop after serving a single pipe session.\n"
        << "  --max-sessions <count>  Stop after serving the specified number of sessions.\n"
        << "  --fb2cng-exe <path>     Use built-in fb2cng converter profile.\n"
        << "  --fb2cng-config <path>  Built-in fb2cng configuration path.\n";
}

#ifdef _WIN32
struct SHostShutdownState
{
    std::atomic<bool> Requested = false;
    std::atomic<int> ExitCode = 0;
};

void TryWakePipeServer(const std::filesystem::path& pipePath) noexcept
{
    try
    {
        auto connection = Librova::PipeTransport::ConnectToNamedPipe(pipePath, std::chrono::milliseconds(250));
        (void)connection;
    }
    catch (...)
    {
    }
}

[[nodiscard]] std::jthread StartParentProcessWatchdog(
    const std::optional<std::uint32_t> parentProcessId,
    const std::filesystem::path& pipePath,
    SHostShutdownState& shutdownState)
{
    if (!parentProcessId.has_value())
    {
        return {};
    }

    HANDLE parentHandle = ::OpenProcess(SYNCHRONIZE, FALSE, *parentProcessId);
    if (parentHandle == nullptr)
    {
        throw std::runtime_error("Failed to open parent process handle for --parent-pid.");
    }

    return std::jthread([parentHandle, parentProcessId, pipePath, &shutdownState](std::stop_token stopToken)
    {
        while (!stopToken.stop_requested())
        {
            const auto waitResult = ::WaitForSingleObject(parentHandle, 250);
            if (waitResult == WAIT_TIMEOUT)
            {
                continue;
            }

            if (waitResult == WAIT_OBJECT_0)
            {
                Librova::Logging::Warn(
                    "Parent process {} exited. Stopping Librova.Core.Host.",
                    *parentProcessId);
                shutdownState.ExitCode.store(0);
                shutdownState.Requested.store(true);
                TryWakePipeServer(pipePath);
                break;
            }

            Librova::Logging::Warn(
                "Failed while waiting for parent process {}. WaitResult={} LastError={}.",
                *parentProcessId,
                static_cast<unsigned long>(waitResult),
                static_cast<unsigned long>(::GetLastError()));
            shutdownState.ExitCode.store(1);
            shutdownState.Requested.store(true);
            TryWakePipeServer(pipePath);
            break;
        }

        ::CloseHandle(parentHandle);
    });
}

[[nodiscard]] std::jthread StartShutdownEventWatchdog(
    const std::optional<std::string>& shutdownEventName,
    const std::filesystem::path& pipePath,
    SHostShutdownState& shutdownState)
{
    if (!shutdownEventName.has_value())
    {
        return {};
    }

    HANDLE shutdownEvent = ::CreateEventW(nullptr, TRUE, FALSE, Librova::Unicode::Utf8ToWide(*shutdownEventName).c_str());
    if (shutdownEvent == nullptr)
    {
        throw std::runtime_error("Failed to create or open shutdown event handle for --shutdown-event.");
    }

    return std::jthread([shutdownEvent, shutdownEventName, pipePath, &shutdownState](std::stop_token stopToken)
    {
        while (!stopToken.stop_requested())
        {
            const auto waitResult = ::WaitForSingleObject(shutdownEvent, 250);
            if (waitResult == WAIT_TIMEOUT)
            {
                continue;
            }

            if (waitResult == WAIT_OBJECT_0)
            {
                Librova::Logging::Info(
                    "Shutdown requested by UI event '{}'.",
                    *shutdownEventName);
                shutdownState.ExitCode.store(0);
                shutdownState.Requested.store(true);
                TryWakePipeServer(pipePath);
                break;
            }

            Librova::Logging::Warn(
                "Failed while waiting for shutdown event '{}'. WaitResult={} LastError={}.",
                *shutdownEventName,
                static_cast<unsigned long>(waitResult),
                static_cast<unsigned long>(::GetLastError()));
            shutdownState.ExitCode.store(1);
            shutdownState.Requested.store(true);
            TryWakePipeServer(pipePath);
            break;
        }

        ::CloseHandle(shutdownEvent);
    });
}
#endif

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const auto options = Librova::CoreHost::CHostOptions::Parse(CollectArguments(argc, argv));

        if (options.ShowHelp)
        {
            PrintUsage();
            return 0;
        }

        if (options.ShowVersion)
        {
            std::cout << Librova::Core::CVersion::GetValue() << '\n';
            return 0;
        }

        Librova::CoreHost::CLibraryBootstrap::PrepareLibraryRoot(options.LibraryRoot, options.LibraryOpenMode);
        Librova::Logging::CLogging::InitializeHostLogger(GetLogFilePath(options));
        Librova::Logging::Info(
            "Starting Librova.Core.Host for library root '{}'.",
            Librova::Unicode::PathToUtf8(options.LibraryRoot));

#ifdef _WIN32
        SHostShutdownState shutdownState;
        auto shutdownWatchdog = StartShutdownEventWatchdog(options.ShutdownEventName, options.PipePath, shutdownState);
        auto parentWatchdog = StartParentProcessWatchdog(options.ParentProcessId, options.PipePath, shutdownState);
        if (options.ShutdownEventName.has_value())
        {
            Librova::Logging::Info("Watching shutdown event '{}' for host lifetime.", *options.ShutdownEventName);
        }
        if (options.ParentProcessId.has_value())
        {
            Librova::Logging::Info("Watching parent process {} for host lifetime.", *options.ParentProcessId);
        }
#endif

        const auto databasePath = GetDatabasePath(options.LibraryRoot);
        Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

        const Librova::ParserRegistry::CBookParserRegistry parserRegistry;
        Librova::BookDatabase::CSqliteBookRepository bookRepository(databasePath);
        Librova::BookDatabase::CSqliteBookQueryRepository queryRepository(databasePath);
        Librova::ManagedStorage::CManagedFileStorage managedStorage(options.LibraryRoot);
        Librova::ManagedTrash::CManagedTrashService trashService(options.LibraryRoot);
        Librova::RecycleBin::CWindowsRecycleBinService recycleBinService;

        std::optional<Librova::ConverterRuntime::CExternalBookConverter> converter;
        if (const auto profile =
                Librova::ConverterConfiguration::TryBuildCommandProfile(options.ConverterConfiguration);
            profile.has_value())
        {
            const auto libraryLayout = Librova::StoragePlanning::CManagedLibraryLayout::Build(options.LibraryRoot);
            converter.emplace(Librova::ConverterRuntime::SExternalConverterSettings{
                .CommandProfile = *profile,
                .WorkingDirectory =
                    options.ConverterConfiguration.Mode == Librova::ConverterConfiguration::EConverterConfigurationMode::BuiltInFb2Cng
                        ? libraryLayout.LogsDirectory
                        : std::filesystem::path{}
            });
        }

        const auto* converterPtr =
            converter.has_value() ? static_cast<const Librova::Domain::IBookConverter*>(&*converter) : nullptr;
        const Librova::CoverProcessingStb::CStbCoverImageProcessor coverImageProcessor;

        const Librova::Importing::CSingleFileImportCoordinator singleFileImporter(
            parserRegistry,
            bookRepository,
            queryRepository,
            managedStorage,
            converterPtr,
            &coverImageProcessor);
        const Librova::ZipImporting::CZipImportCoordinator zipImportCoordinator(singleFileImporter);
        const Librova::Application::CLibraryImportFacade importFacade(
            singleFileImporter,
            zipImportCoordinator,
            bookRepository,
            {.LibraryRoot = options.LibraryRoot});
        const Librova::Application::CLibraryCatalogFacade catalogFacade(queryRepository, bookRepository);
        const Librova::Application::CLibraryExportFacade exportFacade(bookRepository, options.LibraryRoot, converterPtr);
        const Librova::Application::CLibraryTrashFacade trashFacade(
            bookRepository,
            trashService,
            options.LibraryRoot,
            &recycleBinService);
        const Librova::Jobs::CImportJobRunner jobRunner(importFacade);
        Librova::Jobs::CImportJobManager jobManager(jobRunner);
        Librova::ApplicationJobs::CImportJobService jobService(jobManager);
        Librova::ProtoServices::CLibraryJobServiceAdapter serviceAdapter(
            jobService,
            importFacade,
            catalogFacade,
            exportFacade,
            trashFacade);
        Librova::PipeTransport::CPipeRequestDispatcher dispatcher(serviceAdapter);
        const Librova::PipeHost::CNamedPipeHost host(dispatcher);

        std::size_t servedSessions = 0;
        while (
#ifdef _WIN32
            !shutdownState.Requested.load() &&
#endif
            (options.MaxSessions == 0 || servedSessions < options.MaxSessions))
        {
            try
            {
                Librova::PipeTransport::CNamedPipeServer server(options.PipePath);
                Librova::Logging::Debug(
                    "Waiting for pipe session on '{}'.",
                    Librova::Unicode::PathToUtf8(options.PipePath));
                auto connection = server.WaitForClient();

#ifdef _WIN32
                if (shutdownState.Requested.load())
                {
                    Librova::Logging::Info("Shutdown requested. Ending pipe accept loop.");
                    break;
                }
#endif

                host.RunSingleSession(std::move(connection));

#ifdef _WIN32
                if (shutdownState.Requested.load())
                {
                    Librova::Logging::Info("Shutdown requested after pipe session. Ending pipe accept loop.");
                    break;
                }
#endif

                Librova::Logging::Debug("Pipe session completed successfully.");
                ++servedSessions;
            }
            catch (const std::exception& error)
            {
#ifdef _WIN32
                if (shutdownState.Requested.load())
                {
                    Librova::Logging::Info(
                        "Stopping pipe loop after shutdown request interrupted waiting or session handling.");
                    break;
                }
#endif

                Librova::Logging::Error("Pipe session failed: {}", error.what());
                std::cerr << "Librova.Core.Host session error: " << error.what() << '\n';
            }
        }

        Librova::Logging::Info("Librova.Core.Host stopped after serving {} sessions.", servedSessions);
        Librova::Logging::CLogging::Shutdown();

#ifdef _WIN32
        return shutdownState.ExitCode.load();
#else
        return 0;
#endif
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
