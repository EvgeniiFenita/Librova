#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include "Domain/BookRepository.hpp"
#include "Domain/ServiceContracts.hpp"
#include "ImportRollbackService.hpp"
#include "ImportWorkloadPlanner.hpp"
#include "StructuredProgressMapper.hpp"
#include "Importing/SingleFileImportCoordinator.hpp"
#include "ZipImporting/ZipImportCoordinator.hpp"

namespace Librova::Application {

enum class EImportMode
{
    SingleFile,
    ZipArchive,
    Batch
};

enum class ENoSuccessfulImportReason
{
    None,
    UnsupportedFormat,
    DuplicateRejected
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
    ENoSuccessfulImportReason NoSuccessfulImportReason = ENoSuccessfulImportReason::None;
    bool HasRollbackCleanupResidue = false;
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

struct SImportSourceValidation
{
    std::optional<std::string> BlockingMessage;

    [[nodiscard]] bool HasBlockingError() const noexcept
    {
        return BlockingMessage.has_value() && !BlockingMessage->empty();
    }
};

struct SLibraryImportContext
{
    std::filesystem::path LibraryRoot;
};

class CLibraryImportFacade final
{
public:
    CLibraryImportFacade(
        const Librova::Importing::ISingleFileImporter& singleFileImporter,
        const Librova::ZipImporting::CZipImportCoordinator& zipImportCoordinator,
        Librova::Domain::IBookRepository& bookRepository,
        SLibraryImportContext libraryContext);

    [[nodiscard]] SImportResult Run(
        const SImportRequest& request,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token stopToken) const;
    [[nodiscard]] SImportSourceValidation ValidateImportSources(
        const std::vector<std::filesystem::path>& sourcePaths) const;

private:
    [[nodiscard]] static bool IsZipPath(const std::filesystem::path& path);

    const Librova::Importing::ISingleFileImporter& m_singleFileImporter;
    const Librova::ZipImporting::CZipImportCoordinator& m_zipImportCoordinator;
    CImportWorkloadPlanner m_workloadPlanner;
    CImportRollbackService m_rollbackService;
    Librova::Domain::IBookRepository& m_bookRepository;
    std::filesystem::path m_libraryRoot;
};

} // namespace Librova::Application
