#include "App/CLibraryApplication.hpp"

#include <filesystem>
#include <optional>
#include <stdexcept>

#include "App/ImportJobManager.hpp"
#include "App/ImportJobRunner.hpp"
#include "App/ImportJobService.hpp"
#include "App/LibraryBootstrap.hpp"
#include "App/LibraryCatalogFacade.hpp"
#include "App/LibraryExportFacade.hpp"
#include "App/LibraryImportFacade.hpp"
#include "App/LibraryTrashFacade.hpp"
#include "Converter/ConverterConfiguration.hpp"
#include "Converter/ExternalBookConverter.hpp"
#include "Database/SchemaMigrator.hpp"
#include "Database/SqliteBookCollectionRepository.hpp"
#include "Database/SqliteBookQueryRepository.hpp"
#include "Database/SqliteBookRepository.hpp"
#include "Domain/ServiceContracts.hpp"
#include "Foundation/UnicodeConversion.hpp"
#include "Import/SingleFileImportCoordinator.hpp"
#include "Import/ZipImportCoordinator.hpp"
#include "Parsing/BookParserRegistry.hpp"
#include "Storage/ManagedFileStorage.hpp"
#include "Storage/ManagedLibraryLayout.hpp"
#include "Storage/ManagedTrashService.hpp"
#include "Storage/StbCoverImageProcessor.hpp"
#include "Storage/WindowsRecycleBinService.hpp"

namespace Librova::Application {
namespace {

[[nodiscard]] std::filesystem::path ComputeDatabasePath(const std::filesystem::path& libraryRoot)
{
    return StoragePlanning::CManagedLibraryLayout::Build(libraryRoot).DatabaseDirectory / "librova.db";
}

[[nodiscard]] std::filesystem::path ComputeRuntimeWorkspaceRoot(const SLibraryApplicationConfig& config)
{
    if (!config.RuntimeWorkspaceRoot.empty())
    {
        return config.RuntimeWorkspaceRoot;
    }

    const auto normalized = config.LibraryRoot.lexically_normal();
    const auto keyUtf8 = Unicode::PathToUtf8(normalized);
    const auto runtimeKey = std::to_string(std::hash<std::string>{}(keyUtf8));
    return std::filesystem::temp_directory_path() / "Librova" / "Runtime" / runtimeKey;
}

[[nodiscard]] std::optional<ConverterRuntime::CExternalBookConverter> BuildConverter(
    const SLibraryApplicationConfig& config,
    const std::filesystem::path& converterWorkingDir)
{
    if (!config.BuiltInFb2CngExePath.has_value())
    {
        return std::nullopt;
    }

    const ConverterConfiguration::SConverterConfiguration converterConfig{
        .Mode = ConverterConfiguration::EConverterConfigurationMode::BuiltInFb2Cng,
        .Fb2Cng = {
            .ExecutablePath = *config.BuiltInFb2CngExePath
        }
    };

    const auto profile = ConverterConfiguration::TryBuildCommandProfile(converterConfig);
    if (!profile.has_value())
    {
        return std::nullopt;
    }

    return ConverterRuntime::CExternalBookConverter(ConverterRuntime::SExternalConverterSettings{
        .CommandProfile = *profile,
        .WorkingDirectory = converterWorkingDir
    });
}

} // namespace

struct CLibraryApplication::SImpl
{
    explicit SImpl(const SLibraryApplicationConfig& config)
        : databasePath(ComputeDatabasePath(config.LibraryRoot))
        , runtimeWorkspaceRoot(ComputeRuntimeWorkspaceRoot(config))
        , converterWorkingDir(
              config.ConverterWorkingDirectory.value_or(runtimeWorkspaceRoot / "ConverterWorkspace"))
        , managedStorageStagingRoot(
              config.ManagedStorageStagingRoot.value_or(runtimeWorkspaceRoot / "ManagedStorageStaging"))
        , parserRegistry()
        , coverImageProcessor()
        , recycleBinService()
        , bookRepository(databasePath)
        , queryRepository(databasePath)
        , collectionRepository(databasePath)
        , managedStorage(config.LibraryRoot, managedStorageStagingRoot)
        , trashService(config.LibraryRoot)
        , converter(BuildConverter(config, converterWorkingDir))
        , converterPtr(converter.has_value() ? static_cast<const Domain::IBookConverter*>(&*converter) : nullptr)
        , singleFileImporter(
              parserRegistry,
              bookRepository,
              queryRepository,
              managedStorage,
              converterPtr,
              &coverImageProcessor)
        , zipImportCoordinator(singleFileImporter)
        , importFacade(
              singleFileImporter,
              zipImportCoordinator,
              bookRepository,
              {.LibraryRoot = config.LibraryRoot})
        , catalogFacade(queryRepository, bookRepository, collectionRepository, collectionRepository)
        , exportFacade(bookRepository, config.LibraryRoot, converterPtr, runtimeWorkspaceRoot)
        , trashFacade(bookRepository, trashService, config.LibraryRoot, &recycleBinService)
        , jobRunner(importFacade)
        , jobManager(jobRunner)
        , jobService(jobManager)
    {
    }

    std::filesystem::path databasePath;
    std::filesystem::path runtimeWorkspaceRoot;
    std::filesystem::path converterWorkingDir;
    std::filesystem::path managedStorageStagingRoot;

    ParserRegistry::CBookParserRegistry parserRegistry;
    CoverProcessingStb::CStbCoverImageProcessor coverImageProcessor;
    RecycleBin::CWindowsRecycleBinService recycleBinService;

    BookDatabase::CSqliteBookRepository bookRepository;
    BookDatabase::CSqliteBookQueryRepository queryRepository;
    BookDatabase::CSqliteBookCollectionRepository collectionRepository;

    ManagedStorage::CManagedFileStorage managedStorage;
    ManagedTrash::CManagedTrashService trashService;

    std::optional<ConverterRuntime::CExternalBookConverter> converter;
    const Domain::IBookConverter* converterPtr;

    Importing::CSingleFileImportCoordinator singleFileImporter;
    ZipImporting::CZipImportCoordinator zipImportCoordinator;

    CLibraryImportFacade importFacade;
    CLibraryCatalogFacade catalogFacade;
    CLibraryExportFacade exportFacade;
    CLibraryTrashFacade trashFacade;

    Jobs::CImportJobRunner jobRunner;
    Jobs::CImportJobManager jobManager;
    ApplicationJobs::CImportJobService jobService;
};

CLibraryApplication::CLibraryApplication(const SLibraryApplicationConfig& config)
{
    CLibraryBootstrap::PrepareLibraryRoot(config.LibraryRoot, config.LibraryOpenMode);

    const auto databasePath = ComputeDatabasePath(config.LibraryRoot);
    DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    m_impl = std::make_unique<SImpl>(config);
}

CLibraryApplication::~CLibraryApplication() = default;

SBookListResult CLibraryApplication::ListBooks(const SBookListRequest& request)
{
    return m_impl->catalogFacade.ListBooks(request);
}

std::optional<SBookDetails> CLibraryApplication::GetBookDetails(Domain::SBookId id)
{
    return m_impl->catalogFacade.GetBookDetails(id);
}

SLibraryStatistics CLibraryApplication::GetLibraryStatistics()
{
    return m_impl->catalogFacade.GetLibraryStatistics();
}

std::vector<Domain::SBookCollection> CLibraryApplication::ListCollections()
{
    return m_impl->catalogFacade.ListCollections();
}

Domain::SBookCollection CLibraryApplication::CreateCollection(std::string nameUtf8, std::string iconKey)
{
    return m_impl->catalogFacade.CreateCollection(std::move(nameUtf8), std::move(iconKey));
}

bool CLibraryApplication::DeleteCollection(std::int64_t collectionId)
{
    return m_impl->catalogFacade.DeleteCollection(collectionId);
}

bool CLibraryApplication::AddBookToCollection(Domain::SBookId bookId, std::int64_t collectionId)
{
    return m_impl->catalogFacade.AddBookToCollection(bookId, collectionId);
}

bool CLibraryApplication::RemoveBookFromCollection(Domain::SBookId bookId, std::int64_t collectionId)
{
    return m_impl->catalogFacade.RemoveBookFromCollection(bookId, collectionId);
}

SImportSourceValidation CLibraryApplication::ValidateImportSources(
    const std::vector<std::filesystem::path>& paths)
{
    return m_impl->importFacade.ValidateImportSources(paths);
}

ApplicationJobs::TImportJobId CLibraryApplication::StartImport(const SImportRequest& request)
{
    return m_impl->jobService.Start(request);
}

std::optional<ApplicationJobs::SImportJobSnapshot> CLibraryApplication::GetImportJobSnapshot(
    ApplicationJobs::TImportJobId jobId)
{
    return m_impl->jobService.TryGetSnapshot(jobId);
}

std::optional<ApplicationJobs::SImportJobResult> CLibraryApplication::GetImportJobResult(
    ApplicationJobs::TImportJobId jobId)
{
    return m_impl->jobService.TryGetResult(jobId);
}

bool CLibraryApplication::WaitImportJob(
    ApplicationJobs::TImportJobId jobId,
    std::chrono::milliseconds timeout)
{
    return m_impl->jobService.Wait(jobId, timeout);
}

bool CLibraryApplication::CancelImportJob(ApplicationJobs::TImportJobId jobId)
{
    return m_impl->jobService.Cancel(jobId);
}

bool CLibraryApplication::RemoveImportJob(ApplicationJobs::TImportJobId jobId)
{
    return m_impl->jobService.Remove(jobId);
}

std::optional<std::filesystem::path> CLibraryApplication::ExportBook(const SExportBookRequest& request)
{
    return m_impl->exportFacade.ExportBook(request);
}

std::optional<STrashedBookResult> CLibraryApplication::MoveBookToTrash(Domain::SBookId id)
{
    return m_impl->trashFacade.MoveBookToTrash(id);
}

} // namespace Librova::Application
