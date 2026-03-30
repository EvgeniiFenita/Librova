#pragma once

#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include "Domain/ServiceContracts.hpp"
#include "Importing/SingleFileImportCoordinator.hpp"
#include "ZipImporting/ZipImportCoordinator.hpp"

namespace LibriFlow::Application {

enum class EImportMode
{
    SingleFile,
    ZipArchive
};

struct SImportRequest
{
    std::filesystem::path SourcePath;
    std::filesystem::path WorkingDirectory;
    std::optional<std::string> Sha256Hex;
    bool AllowProbableDuplicates = false;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return !SourcePath.empty() && !WorkingDirectory.empty();
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
    std::optional<LibriFlow::Importing::SSingleFileImportResult> SingleFileResult;
    std::optional<LibriFlow::ZipImporting::SZipImportResult> ZipResult;

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
        const LibriFlow::Importing::ISingleFileImporter& singleFileImporter,
        const LibriFlow::ZipImporting::CZipImportCoordinator& zipImportCoordinator);

    [[nodiscard]] SImportResult Run(
        const SImportRequest& request,
        LibriFlow::Domain::IProgressSink& progressSink,
        std::stop_token stopToken) const;

private:
    [[nodiscard]] static bool IsZipPath(const std::filesystem::path& path);

    const LibriFlow::Importing::ISingleFileImporter& m_singleFileImporter;
    const LibriFlow::ZipImporting::CZipImportCoordinator& m_zipImportCoordinator;
};

} // namespace LibriFlow::Application
