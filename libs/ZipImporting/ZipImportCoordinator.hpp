#pragma once

#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include "Domain/ServiceContracts.hpp"
#include "Importing/SingleFileImportCoordinator.hpp"

namespace LibriFlow::ZipImporting {

enum class EZipEntryImportStatus
{
    Imported,
    UnsupportedEntry,
    NestedArchiveSkipped,
    Failed,
    Cancelled
};

struct SZipImportRequest
{
    std::filesystem::path ZipPath;
    std::filesystem::path WorkingDirectory;
    bool AllowProbableDuplicates = false;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return !ZipPath.empty() && !WorkingDirectory.empty();
    }
};

struct SZipEntryImportResult
{
    std::filesystem::path ArchivePath;
    EZipEntryImportStatus Status = EZipEntryImportStatus::UnsupportedEntry;
    std::optional<LibriFlow::Importing::SSingleFileImportResult> SingleFileResult;
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
    explicit CZipImportCoordinator(const LibriFlow::Importing::ISingleFileImporter& singleFileImporter);

    [[nodiscard]] SZipImportResult Run(
        const SZipImportRequest& request,
        LibriFlow::Domain::IProgressSink& progressSink,
        std::stop_token stopToken) const;

private:
    const LibriFlow::Importing::ISingleFileImporter& m_singleFileImporter;
};

} // namespace LibriFlow::ZipImporting
