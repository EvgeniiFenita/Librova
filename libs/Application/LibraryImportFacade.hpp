#pragma once

#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include "Domain/BookRepository.hpp"
#include "Domain/ServiceContracts.hpp"
#include "Importing/SingleFileImportCoordinator.hpp"
#include "ZipImporting/ZipImportCoordinator.hpp"

namespace Librova::Application {

enum class EImportMode
{
    SingleFile,
    ZipArchive,
    Batch
};

struct SImportRequest
{
    std::vector<std::filesystem::path> SourcePaths;
    std::filesystem::path WorkingDirectory;
    std::optional<std::string> Sha256Hex;
    bool AllowProbableDuplicates = false;
    bool ForceEpubConversion = false;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return !SourcePaths.empty() && !WorkingDirectory.empty();
    }
};

struct SImportSummary
{
    EImportMode Mode = EImportMode::SingleFile;
    std::size_t TotalEntries = 0;
    std::size_t ImportedEntries = 0;
    std::size_t FailedEntries = 0;
    std::size_t SkippedEntries = 0;
    std::vector<std::string> Warnings;
};

struct SImportResult
{
    SImportSummary Summary;
    std::optional<Librova::Importing::SSingleFileImportResult> SingleFileResult;
    std::optional<Librova::ZipImporting::SZipImportResult> ZipResult;
    std::vector<Librova::Domain::SBookId> ImportedBookIds;
    bool WasCancelled = false;

    [[nodiscard]] bool IsSuccess() const noexcept
    {
        return Summary.ImportedEntries > 0 && Summary.FailedEntries == 0;
    }

    [[nodiscard]] bool IsPartialSuccess() const noexcept
    {
        return Summary.ImportedEntries > 0 && Summary.FailedEntries > 0;
    }
};

class CLibraryImportFacade final
{
public:
    CLibraryImportFacade(
        const Librova::Importing::ISingleFileImporter& singleFileImporter,
        const Librova::ZipImporting::CZipImportCoordinator& zipImportCoordinator,
        Librova::Domain::IBookRepository* bookRepository = nullptr,
        std::filesystem::path libraryRoot = {});

    [[nodiscard]] SImportResult Run(
        const SImportRequest& request,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token stopToken) const;

private:
    [[nodiscard]] static bool IsZipPath(const std::filesystem::path& path);
    void RollbackImportedBooks(const std::vector<Librova::Domain::SBookId>& importedBookIds) const;

    const Librova::Importing::ISingleFileImporter& m_singleFileImporter;
    const Librova::ZipImporting::CZipImportCoordinator& m_zipImportCoordinator;
    Librova::Domain::IBookRepository* m_bookRepository = nullptr;
    std::filesystem::path m_libraryRoot;
};

} // namespace Librova::Application
