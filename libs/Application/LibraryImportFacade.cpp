#include "Application/LibraryImportFacade.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace {

[[nodiscard]] std::string ToLower(std::string value)
{
    std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

} // namespace

namespace LibriFlow::Application {

CLibraryImportFacade::CLibraryImportFacade(
    const LibriFlow::Importing::ISingleFileImporter& singleFileImporter,
    const LibriFlow::ZipImporting::CZipImportCoordinator& zipImportCoordinator)
    : m_singleFileImporter(singleFileImporter)
    , m_zipImportCoordinator(zipImportCoordinator)
{
}

bool CLibraryImportFacade::IsZipPath(const std::filesystem::path& path)
{
    return ToLower(path.extension().string()) == ".zip";
}

SImportResult CLibraryImportFacade::Run(
    const SImportRequest& request,
    LibriFlow::Domain::IProgressSink& progressSink,
    const std::stop_token stopToken) const
{
    if (!request.IsValid())
    {
        throw std::invalid_argument("Import request must contain source path and working directory.");
    }

    if (IsZipPath(request.SourcePath))
    {
        const auto zipResult = m_zipImportCoordinator.Run({
            .ZipPath = request.SourcePath,
            .WorkingDirectory = request.WorkingDirectory,
            .AllowProbableDuplicates = request.AllowProbableDuplicates
        }, progressSink, stopToken);

        SImportResult result;
        result.Summary.Mode = EImportMode::ZipArchive;
        result.Summary.TotalEntries = zipResult.Entries.size();
        result.Summary.ImportedEntries = zipResult.ImportedCount();
        result.ZipResult = zipResult;

        for (const auto& entry : zipResult.Entries)
        {
            switch (entry.Status)
            {
            case LibriFlow::ZipImporting::EZipEntryImportStatus::Imported:
                break;
            case LibriFlow::ZipImporting::EZipEntryImportStatus::UnsupportedEntry:
            case LibriFlow::ZipImporting::EZipEntryImportStatus::NestedArchiveSkipped:
                ++result.Summary.SkippedEntries;
                if (!entry.Error.empty())
                {
                    result.Summary.Warnings.push_back(entry.Error);
                }
                break;
            case LibriFlow::ZipImporting::EZipEntryImportStatus::Failed:
            case LibriFlow::ZipImporting::EZipEntryImportStatus::Cancelled:
                ++result.Summary.FailedEntries;
                if (entry.SingleFileResult.has_value())
                {
                    result.Summary.Warnings.insert(
                        result.Summary.Warnings.end(),
                        entry.SingleFileResult->Warnings.begin(),
                        entry.SingleFileResult->Warnings.end());
                    if (!entry.SingleFileResult->Error.empty())
                    {
                        result.Summary.Warnings.push_back(entry.SingleFileResult->Error);
                    }
                }
                else if (!entry.Error.empty())
                {
                    result.Summary.Warnings.push_back(entry.Error);
                }
                break;
            }
        }

        return result;
    }

    const auto singleFileResult = m_singleFileImporter.Run({
        .SourcePath = request.SourcePath,
        .WorkingDirectory = request.WorkingDirectory,
        .Sha256Hex = request.Sha256Hex,
        .AllowProbableDuplicates = request.AllowProbableDuplicates
    }, progressSink, stopToken);

    SImportResult result;
    result.Summary.Mode = EImportMode::SingleFile;
    result.Summary.TotalEntries = 1;
    result.SingleFileResult = singleFileResult;

    if (singleFileResult.IsSuccess())
    {
        result.Summary.ImportedEntries = 1;
    }
    else
    {
        result.Summary.FailedEntries = 1;
        result.Summary.Warnings = singleFileResult.Warnings;

        if (!singleFileResult.Error.empty())
        {
            result.Summary.Warnings.push_back(singleFileResult.Error);
        }
    }

    return result;
}

} // namespace LibriFlow::Application
