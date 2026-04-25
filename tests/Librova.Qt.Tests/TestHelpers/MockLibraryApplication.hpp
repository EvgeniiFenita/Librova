#pragma once

#include <optional>
#include <vector>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "App/ILibraryApplication.hpp"

namespace LibrovaQt::Tests {

// Controllable stub implementing ILibraryApplication for unit tests.
// All methods return the public result fields; override individual calls as needed.
class MockLibraryApplication : public Librova::Application::ILibraryApplication
{
public:
    // ── Configurable results ──────────────────────────────────────────────────

    Librova::Application::SBookListResult listBooksResult;
    std::vector<Librova::Application::SBookListResult> queuedListBooksResults;
    std::optional<Librova::Application::SBookDetails> getBookDetailsResult;
    Librova::Application::SLibraryStatistics libraryStatistics;
    std::vector<Librova::Domain::SBookCollection> listCollectionsResult;
    Librova::Domain::SBookCollection createCollectionResult;
    bool deleteCollectionResult = true;
    bool addBookToCollectionResult = true;
    bool throwOnAddBookToCollection = false;
    bool throwOnValidateImportSources = false;
    bool removeBookFromCollectionResult = true;
    Librova::Application::SImportSourceValidation validateSourcesResult;
    Librova::ApplicationJobs::TImportJobId startImportResult = 1;
    std::optional<Librova::ApplicationJobs::SImportJobSnapshot> jobSnapshot;
    std::optional<Librova::ApplicationJobs::SImportJobResult> jobResult;
    bool cancelJobResult = true;
    bool removeJobResult = true;
    std::optional<std::filesystem::path> exportResult;
    std::optional<Librova::Application::STrashedBookResult> trashResult;
    std::optional<Librova::Application::SImportRequest> lastStartImportRequest;
    std::optional<Librova::Application::SExportBookRequest> lastExportBookRequest;
    std::optional<Librova::Application::SBookListRequest> lastListBooksRequest;
    std::optional<std::int64_t> lastDeleteCollectionId;
    std::optional<std::string> lastCreateCollectionName;
    std::optional<std::string> lastCreateCollectionIconKey;
    std::optional<Librova::Domain::SBookId> lastAddedBookId;
    std::optional<std::int64_t> lastAddedCollectionId;

    // ── ILibraryApplication ───────────────────────────────────────────────────

    Librova::Application::SBookListResult ListBooks(
        const Librova::Application::SBookListRequest& request) override
    {
        lastListBooksRequest = request;
        if (!queuedListBooksResults.empty())
        {
            auto result = queuedListBooksResults.front();
            queuedListBooksResults.erase(queuedListBooksResults.begin());
            return result;
        }
        return listBooksResult;
    }

    std::optional<Librova::Application::SBookDetails> GetBookDetails(
        Librova::Domain::SBookId) override
    {
        return getBookDetailsResult;
    }

    Librova::Application::SLibraryStatistics GetLibraryStatistics() override
    {
        return libraryStatistics;
    }

    std::vector<Librova::Domain::SBookCollection> ListCollections() override
    {
        return listCollectionsResult;
    }

    Librova::Domain::SBookCollection CreateCollection(
        std::string name, std::string iconKey) override
    {
        lastCreateCollectionName = std::move(name);
        lastCreateCollectionIconKey = std::move(iconKey);
        return createCollectionResult;
    }

    bool DeleteCollection(std::int64_t id) override
    {
        lastDeleteCollectionId = id;
        return deleteCollectionResult;
    }

    bool AddBookToCollection(Librova::Domain::SBookId bookId, std::int64_t collectionId) override
    {
        lastAddedBookId = bookId;
        lastAddedCollectionId = collectionId;
        if (throwOnAddBookToCollection)
        {
            throw std::runtime_error("add failed");
        }
        return addBookToCollectionResult;
    }

    bool RemoveBookFromCollection(Librova::Domain::SBookId, std::int64_t) override
    {
        return removeBookFromCollectionResult;
    }

    Librova::Application::SImportSourceValidation ValidateImportSources(
        const std::vector<std::filesystem::path>&) override
    {
        if (throwOnValidateImportSources)
        {
            throw std::runtime_error("validation failed");
        }
        return validateSourcesResult;
    }

    Librova::ApplicationJobs::TImportJobId StartImport(
        const Librova::Application::SImportRequest& request) override
    {
        lastStartImportRequest = request;
        return startImportResult;
    }

    std::optional<Librova::ApplicationJobs::SImportJobSnapshot> GetImportJobSnapshot(
        Librova::ApplicationJobs::TImportJobId) override
    {
        return jobSnapshot;
    }

    std::optional<Librova::ApplicationJobs::SImportJobResult> GetImportJobResult(
        Librova::ApplicationJobs::TImportJobId) override
    {
        return jobResult;
    }

    bool WaitImportJob(
        Librova::ApplicationJobs::TImportJobId,
        std::chrono::milliseconds) override
    {
        return true;
    }

    bool CancelImportJob(Librova::ApplicationJobs::TImportJobId) override
    {
        return cancelJobResult;
    }

    bool RemoveImportJob(Librova::ApplicationJobs::TImportJobId) override
    {
        return removeJobResult;
    }

    std::optional<std::filesystem::path> ExportBook(
        const Librova::Application::SExportBookRequest& request) override
    {
        lastExportBookRequest = request;
        return exportResult;
    }

    std::optional<Librova::Application::STrashedBookResult> MoveBookToTrash(
        Librova::Domain::SBookId) override
    {
        return trashResult;
    }
};

} // namespace LibrovaQt::Tests
