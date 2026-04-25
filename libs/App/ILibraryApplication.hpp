#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "App/ImportJobService.hpp"
#include "App/LibraryCatalogFacade.hpp"
#include "App/LibraryExportFacade.hpp"
#include "App/LibraryImportFacade.hpp"
#include "App/LibraryTrashFacade.hpp"
#include "Domain/BookCollection.hpp"

namespace Librova::Application {

enum class ELibraryOpenMode
{
    Open,      // open an existing Librova-managed library root
    CreateNew  // create a new managed library in a new or empty directory
};

struct SLibraryApplicationConfig
{
    std::filesystem::path LibraryRoot;
    std::filesystem::path RuntimeWorkspaceRoot;
    std::filesystem::path LogFilePath;
    std::optional<std::filesystem::path> BuiltInFb2CngExePath;
    std::optional<std::filesystem::path> ConverterWorkingDirectory;
    std::optional<std::filesystem::path> ManagedStorageStagingRoot;
    ELibraryOpenMode LibraryOpenMode = ELibraryOpenMode::Open;
};

class ILibraryApplication
{
public:
    virtual ~ILibraryApplication() = default;

    virtual SBookListResult ListBooks(const SBookListRequest& request) = 0;
    virtual std::optional<SBookDetails> GetBookDetails(Domain::SBookId id) = 0;
    virtual SLibraryStatistics GetLibraryStatistics() = 0;

    virtual std::vector<Domain::SBookCollection> ListCollections() = 0;
    virtual Domain::SBookCollection CreateCollection(std::string nameUtf8, std::string iconKey) = 0;
    virtual bool DeleteCollection(std::int64_t collectionId) = 0;
    virtual bool AddBookToCollection(Domain::SBookId bookId, std::int64_t collectionId) = 0;
    virtual bool RemoveBookFromCollection(Domain::SBookId bookId, std::int64_t collectionId) = 0;

    virtual SImportSourceValidation ValidateImportSources(const std::vector<std::filesystem::path>& paths) = 0;
    virtual ApplicationJobs::TImportJobId StartImport(const SImportRequest& request) = 0;
    virtual std::optional<ApplicationJobs::SImportJobSnapshot> GetImportJobSnapshot(
        ApplicationJobs::TImportJobId jobId) = 0;
    virtual std::optional<ApplicationJobs::SImportJobResult> GetImportJobResult(
        ApplicationJobs::TImportJobId jobId) = 0;
    // Test/backend-harness helper only. Production Qt adapters must not call this through the
    // serialized executor, as it would block the thread handling job completion/progress delivery.
    virtual bool WaitImportJob(
        ApplicationJobs::TImportJobId jobId,
        std::chrono::milliseconds timeout) = 0;
    virtual bool CancelImportJob(ApplicationJobs::TImportJobId jobId) = 0;
    virtual bool RemoveImportJob(ApplicationJobs::TImportJobId jobId) = 0;

    virtual std::optional<std::filesystem::path> ExportBook(const SExportBookRequest& request) = 0;
    virtual std::optional<STrashedBookResult> MoveBookToTrash(Domain::SBookId id) = 0;
};

} // namespace Librova::Application
