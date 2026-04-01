#pragma once

#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include "Domain/ServiceContracts.hpp"
#include "Importing/SingleFileImportCoordinator.hpp"

namespace Librova::ZipImporting {

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
