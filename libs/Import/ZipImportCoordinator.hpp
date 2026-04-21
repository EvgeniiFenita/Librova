#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include "Domain/ServiceContracts.hpp"
#include "Import/ImportPerfTracker.hpp"
#include "Import/SingleFileImportCoordinator.hpp"

namespace Librova::ZipImporting {

enum class EZipEntryImportStatus
{
    Imported,
    Skipped,
    UnsupportedEntry,
    NestedArchiveSkipped,
    Failed,
    Cancelled
};

struct SZipImportRequest
{
    std::filesystem::path ZipPath;
    std::filesystem::path WorkingDirectory;
    std::uint64_t JobId = 0;
    bool AllowProbableDuplicates = false;
    bool ForceEpubConversion = false;
    bool ImportCovers = true;
    std::vector<Librova::Domain::SBookId> ReservedBookIds; // optional pre-reserved IDs; mapped by import index
    std::optional<std::reference_wrapper<Librova::Domain::IBookRepository>> WriterRepository; // optional writer-dispatching repo for parallel writes
    std::optional<std::reference_wrapper<Librova::Importing::CImportPerfTracker>> PerfTracker;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return !ZipPath.empty() && !WorkingDirectory.empty();
    }
};

struct SZipEntryImportResult
{
    std::filesystem::path ArchivePath;
    EZipEntryImportStatus Status = EZipEntryImportStatus::UnsupportedEntry;
    std::optional<Librova::Importing::SSingleFileImportResult> SingleFileResult;
    std::string Error;

    [[nodiscard]] bool IsImported() const noexcept
    {
        return Status == EZipEntryImportStatus::Imported
            && SingleFileResult.has_value()
            && SingleFileResult->IsSuccess();
    }
};

struct SZipImportResult
{
    std::vector<SZipEntryImportResult> Entries;

    [[nodiscard]] std::size_t ImportedCount() const noexcept;
};

class CZipImportCoordinator final
{
public:
    explicit CZipImportCoordinator(const Librova::Importing::ISingleFileImporter& singleFileImporter);

    [[nodiscard]] std::size_t CountPlannedEntries(
        const std::filesystem::path& zipPath,
        const Librova::Domain::IProgressSink* progressSink = nullptr,
        std::stop_token stopToken = {}) const;

    [[nodiscard]] SZipImportResult Run(
        const SZipImportRequest& request,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token stopToken) const;

private:
    const Librova::Importing::ISingleFileImporter& m_singleFileImporter;
};

} // namespace Librova::ZipImporting
