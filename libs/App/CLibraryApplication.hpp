#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "App/ILibraryApplication.hpp"

namespace Librova::Application {

class CLibraryApplication final : public ILibraryApplication
{
public:
    explicit CLibraryApplication(const SLibraryApplicationConfig& config);
    ~CLibraryApplication() override;

    CLibraryApplication(const CLibraryApplication&) = delete;
    CLibraryApplication& operator=(const CLibraryApplication&) = delete;
    CLibraryApplication(CLibraryApplication&&) = delete;
    CLibraryApplication& operator=(CLibraryApplication&&) = delete;

    SBookListResult ListBooks(const SBookListRequest& request) override;
    std::optional<SBookDetails> GetBookDetails(Domain::SBookId id) override;
    SLibraryStatistics GetLibraryStatistics() override;

    std::vector<Domain::SBookCollection> ListCollections() override;
    Domain::SBookCollection CreateCollection(std::string nameUtf8, std::string iconKey) override;
    bool DeleteCollection(std::int64_t collectionId) override;
    bool AddBookToCollection(Domain::SBookId bookId, std::int64_t collectionId) override;
    bool RemoveBookFromCollection(Domain::SBookId bookId, std::int64_t collectionId) override;

    SImportSourceValidation ValidateImportSources(const std::vector<std::filesystem::path>& paths) override;
    ApplicationJobs::TImportJobId StartImport(const SImportRequest& request) override;
    std::optional<ApplicationJobs::SImportJobSnapshot> GetImportJobSnapshot(
        ApplicationJobs::TImportJobId jobId) override;
    std::optional<ApplicationJobs::SImportJobResult> GetImportJobResult(
        ApplicationJobs::TImportJobId jobId) override;
    bool WaitImportJob(ApplicationJobs::TImportJobId jobId, std::chrono::milliseconds timeout) override;
    bool CancelImportJob(ApplicationJobs::TImportJobId jobId) override;
    bool RemoveImportJob(ApplicationJobs::TImportJobId jobId) override;

    std::optional<std::filesystem::path> ExportBook(const SExportBookRequest& request) override;
    std::optional<STrashedBookResult> MoveBookToTrash(Domain::SBookId id) override;

private:
    struct SImpl;
    std::unique_ptr<SImpl> m_impl;
};

} // namespace Librova::Application
