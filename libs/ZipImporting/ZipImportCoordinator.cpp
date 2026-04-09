#include "ZipImporting/ZipImportCoordinator.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include <zip.h>

#include "Logging/Logging.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace {

[[nodiscard]] std::string JoinWarningsAndError(
    const std::vector<std::string>& warnings,
    const std::string& error)
{
    std::string combined;

    for (const auto& warning : warnings)
    {
        if (warning.empty())
        {
            continue;
        }

        if (!combined.empty())
        {
            combined += " | ";
        }

        combined += warning;
    }

    if (!error.empty())
    {
        if (!combined.empty())
        {
            combined += " | ";
        }

        combined += error;
    }

    return combined;
}

[[nodiscard]] std::string GetSingleFileLogReason(const Librova::Importing::SSingleFileImportResult& result)
{
    return JoinWarningsAndError(
        result.Warnings,
        result.DiagnosticError.empty() ? result.Error : result.DiagnosticError);
}

[[nodiscard]] bool IsSkippedSingleFileResult(const Librova::Importing::SSingleFileImportResult& result) noexcept
{
    return result.Status == Librova::Importing::ESingleFileImportStatus::RejectedDuplicate
        || result.Status == Librova::Importing::ESingleFileImportStatus::DecisionRequired
        || result.Status == Librova::Importing::ESingleFileImportStatus::UnsupportedFormat;
}

[[nodiscard]] std::string_view GetSingleFileStage(const Librova::Importing::SSingleFileImportResult& result) noexcept
{
    return result.Status == Librova::Importing::ESingleFileImportStatus::RejectedDuplicate
            || result.Status == Librova::Importing::ESingleFileImportStatus::DecisionRequired
        ? "duplicate-check"
        : "single-file-import";
}

[[nodiscard]] std::string_view GetSingleFileStatus(const Librova::Importing::SSingleFileImportResult& result) noexcept
{
    switch (result.Status)
    {
    case Librova::Importing::ESingleFileImportStatus::RejectedDuplicate:
        return "rejected-duplicate";
    case Librova::Importing::ESingleFileImportStatus::DecisionRequired:
        return "decision-required";
    case Librova::Importing::ESingleFileImportStatus::UnsupportedFormat:
        return "unsupported-format";
    case Librova::Importing::ESingleFileImportStatus::Cancelled:
        return "cancelled";
    case Librova::Importing::ESingleFileImportStatus::Failed:
        return "failed";
    case Librova::Importing::ESingleFileImportStatus::Imported:
        return "imported";
    }

    return "failed";
}

void LogZipEntryIssueIfInitialized(
    const std::filesystem::path& zipPath,
    const std::filesystem::path& entryPath,
    std::string_view stage,
    std::string_view outcome,
    std::string_view status,
    std::string_view reason)
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    const auto utf8ZipPath = Librova::Unicode::PathToUtf8(zipPath);
    const auto utf8EntryPath = Librova::Unicode::PathToUtf8(entryPath);
    if (outcome == "failed")
    {
        Librova::Logging::Error(
            "ZIP entry failed: archive='{}' entry='{}' stage='{}' outcome='{}' status='{}' reason='{}'",
            utf8ZipPath,
            utf8EntryPath,
            stage,
            outcome,
            status,
            reason);
        return;
    }

    Librova::Logging::Warn(
        "ZIP entry skipped: archive='{}' entry='{}' stage='{}' outcome='{}' status='{}' reason='{}'",
        utf8ZipPath,
        utf8EntryPath,
        stage,
        outcome,
        status,
        reason);
}

class CZipArchive final
{
public:
    explicit CZipArchive(const std::filesystem::path& filePath)
    {
        int errorCode = ZIP_ER_OK;
        const std::string utf8Path = Librova::Unicode::PathToUtf8(filePath);
        m_archive = zip_open(utf8Path.c_str(), ZIP_RDONLY, &errorCode);

        if (m_archive == nullptr)
        {
            zip_error_t errorState;
            zip_error_init_with_code(&errorState, errorCode);
            const std::string message = zip_error_strerror(&errorState);
            zip_error_fini(&errorState);
            throw std::runtime_error("Failed to open ZIP archive: " + message);
        }
    }

    ~CZipArchive()
    {
        if (m_archive != nullptr)
        {
            zip_close(m_archive);
        }
    }

    CZipArchive(const CZipArchive&) = delete;
    CZipArchive& operator=(const CZipArchive&) = delete;
    CZipArchive(CZipArchive&&) = delete;
    CZipArchive& operator=(CZipArchive&&) = delete;

    [[nodiscard]] zip_int64_t GetEntryCount() const
    {
        return zip_get_num_entries(m_archive, 0);
    }

    [[nodiscard]] std::string GetEntryName(const zip_uint64_t index) const
    {
        const char* name = zip_get_name(m_archive, index, ZIP_FL_ENC_GUESS);

        if (name == nullptr)
        {
            throw std::runtime_error("Failed to read ZIP entry name.");
        }

        return name;
    }

    [[nodiscard]] std::vector<std::byte> ReadBytes(const zip_uint64_t index) const
    {
        zip_stat_t stat{};

        if (zip_stat_index(m_archive, index, 0, &stat) != 0)
        {
            throw std::runtime_error("Failed to stat ZIP entry.");
        }

        zip_file_t* file = zip_fopen_index(m_archive, index, 0);

        if (file == nullptr)
        {
            throw std::runtime_error("Failed to open ZIP entry.");
        }

        std::vector<std::byte> bytes(static_cast<std::size_t>(stat.size));
        const zip_int64_t readCount = zip_fread(file, bytes.data(), stat.size);
        zip_fclose(file);

        if (readCount < 0 || static_cast<zip_uint64_t>(readCount) != stat.size)
        {
            throw std::runtime_error("Failed to read ZIP entry payload.");
        }

        return bytes;
    }

private:
    zip_t* m_archive = nullptr;
};

static_assert(!std::is_copy_constructible_v<CZipArchive>);
static_assert(!std::is_move_constructible_v<CZipArchive>);

[[nodiscard]] std::string ToLower(std::string value)
{
    std::ranges::transform(value, value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

[[nodiscard]] bool IsDirectoryEntry(const std::string_view entryPath)
{
    return !entryPath.empty() && entryPath.back() == '/';
}

[[nodiscard]] bool IsNestedArchive(const std::filesystem::path& entryPath)
{
    return ToLower(entryPath.extension().string()) == ".zip";
}

[[nodiscard]] bool IsSupportedBookEntry(const std::filesystem::path& entryPath)
{
    const std::string extension = ToLower(entryPath.extension().string());
    return extension == ".epub" || extension == ".fb2";
}

[[nodiscard]] bool IsSafeRelativeEntryPath(const std::filesystem::path& entryPath)
{
    if (entryPath.empty() || entryPath.is_absolute() || entryPath.has_root_name() || entryPath.has_root_directory())
    {
        return false;
    }

    for (const auto& part : entryPath)
    {
        if (part == "..")
        {
            return false;
        }
    }

    return true;
}

void EnsureDirectory(const std::filesystem::path& path)
{
    std::error_code errorCode;
    std::filesystem::create_directories(path, errorCode);

    if (errorCode)
    {
        throw std::runtime_error(
            std::string{"Failed to create directory: "} + Librova::Unicode::PathToUtf8(path));
    }
}

void RemovePathNoThrow(const std::filesystem::path& path) noexcept
{
    if (path.empty())
    {
        return;
    }

    std::error_code errorCode;
    std::filesystem::remove_all(path, errorCode);
}

void RemoveEmptyDirectoriesUpToRootNoThrow(
    const std::filesystem::path& rootPath,
    std::filesystem::path currentPath) noexcept
{
    if (rootPath.empty() || currentPath.empty())
    {
        return;
    }

    const std::filesystem::path normalizedRootPath = rootPath.lexically_normal();

    std::error_code errorCode;
    while (!currentPath.empty() && currentPath.lexically_normal() != normalizedRootPath.parent_path())
    {
        std::filesystem::remove(currentPath, errorCode);
        if (errorCode)
        {
            errorCode.clear();
            break;
        }

        if (currentPath.lexically_normal() == normalizedRootPath)
        {
            break;
        }

        currentPath = currentPath.parent_path();
    }
}

void CleanupExtractedEntryNoThrow(
    const std::filesystem::path& workingDirectory,
    const std::filesystem::path& extractedPath) noexcept
{
    if (extractedPath.empty())
    {
        return;
    }

    const std::filesystem::path extractedRoot = workingDirectory / "extracted";
    RemovePathNoThrow(extractedPath);
    RemoveEmptyDirectoriesUpToRootNoThrow(extractedRoot, extractedPath.parent_path());
}

void CleanupEntryWorkspaceNoThrow(
    const std::filesystem::path& workingDirectory,
    const std::filesystem::path& entryWorkingDirectory) noexcept
{
    if (entryWorkingDirectory.empty())
    {
        return;
    }

    const std::filesystem::path entriesRoot = workingDirectory / "entries";
    RemovePathNoThrow(entryWorkingDirectory);
    RemoveEmptyDirectoriesUpToRootNoThrow(entriesRoot, entryWorkingDirectory.parent_path());
    RemovePathNoThrow(entriesRoot);
}

std::filesystem::path ExtractEntryToWorkspace(
    const CZipArchive& archive,
    const zip_uint64_t index,
    const std::string_view entryName,
    const std::filesystem::path& workingDirectory)
{
    const std::filesystem::path relativePath = std::filesystem::path{entryName}.lexically_normal();

    if (!IsSafeRelativeEntryPath(relativePath))
    {
        throw std::runtime_error("ZIP entry path is unsafe.");
    }

    const std::filesystem::path destinationPath = workingDirectory / "extracted" / relativePath;
    EnsureDirectory(destinationPath.parent_path());

    const std::vector<std::byte> bytes = archive.ReadBytes(index);
    std::ofstream output(destinationPath, std::ios::binary);

    if (!output)
    {
        throw std::runtime_error("Failed to create extracted ZIP entry file.");
    }

    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return destinationPath;
}

class CScopedStructuredProgressSink final : public Librova::Domain::IProgressSink, public Librova::Domain::IStructuredImportProgressSink
{
public:
    CScopedStructuredProgressSink(
        Librova::Domain::IProgressSink& innerSink,
        const std::size_t totalEntries,
        const std::size_t processedEntries,
        const std::size_t contributionEntries)
        : m_innerSink(innerSink)
        , m_totalEntries(totalEntries)
        , m_processedEntries(processedEntries)
        , m_contributionEntries(std::max<std::size_t>(contributionEntries, 1))
    {
    }

    void ReportValue(const int percent, std::string_view message) override
    {
        m_innerSink.ReportValue(MapPercent(percent), message);
    }

    bool IsCancellationRequested() const override
    {
        return m_innerSink.IsCancellationRequested();
    }

    void ReportStructuredProgress(
        const std::size_t totalEntries,
        const std::size_t processedEntries,
        const std::size_t importedEntries,
        const std::size_t failedEntries,
        const std::size_t skippedEntries,
        const int percent,
        std::string_view message) override
    {
        if (auto* structuredSink = dynamic_cast<Librova::Domain::IStructuredImportProgressSink*>(&m_innerSink); structuredSink != nullptr)
        {
            structuredSink->ReportStructuredProgress(
                m_totalEntries,
                m_processedEntries + processedEntries,
                importedEntries,
                failedEntries,
                skippedEntries,
                MapPercent(percent),
                message);
            return;
        }

        m_innerSink.ReportValue(MapPercent(percent), message);
    }

private:
    [[nodiscard]] int MapPercent(const int localPercent) const noexcept
    {
        const auto clampedLocalPercent = std::clamp(localPercent, 0, 100);
        const auto scaledProgress = static_cast<long long>(m_processedEntries * 100)
            + (static_cast<long long>(m_contributionEntries) * clampedLocalPercent);
        const auto denominator = static_cast<long long>(std::max<std::size_t>(m_totalEntries, 1)) * 100LL;
        return static_cast<int>(std::clamp((scaledProgress * 100LL) / denominator, 0LL, 100LL));
    }

    Librova::Domain::IProgressSink& m_innerSink;
    std::size_t m_totalEntries = 0;
    std::size_t m_processedEntries = 0;
    std::size_t m_contributionEntries = 1;
};

} // namespace

namespace Librova::ZipImporting {

std::size_t SZipImportResult::ImportedCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(Entries.begin(), Entries.end(), [](const auto& entry) {
        return entry.IsImported();
    }));
}

CZipImportCoordinator::CZipImportCoordinator(const Librova::Importing::ISingleFileImporter& singleFileImporter)
    : m_singleFileImporter(singleFileImporter)
{
}

std::size_t CZipImportCoordinator::CountPlannedEntries(
    const std::filesystem::path& zipPath,
    const Librova::Domain::IProgressSink* progressSink,
    const std::stop_token stopToken) const
{
    CZipArchive archive(zipPath);
    std::size_t plannedEntries = 0;
    const zip_uint64_t entryCount = static_cast<zip_uint64_t>(archive.GetEntryCount());

    for (zip_uint64_t index = 0; index < entryCount; ++index)
    {
        if (stopToken.stop_requested() || (progressSink != nullptr && progressSink->IsCancellationRequested()))
        {
            break;
        }

        const std::string entryName = archive.GetEntryName(index);
        if (!IsDirectoryEntry(entryName))
        {
            ++plannedEntries;
        }
    }

    return plannedEntries;
}

SZipImportResult CZipImportCoordinator::Run(
    const SZipImportRequest& request,
    Librova::Domain::IProgressSink& progressSink,
    const std::stop_token stopToken) const
{
    if (!request.IsValid())
    {
        throw std::invalid_argument("ZIP import request must contain archive path and working directory.");
    }

    CZipArchive archive(request.ZipPath);
    SZipImportResult result;
    const zip_uint64_t entryCount = static_cast<zip_uint64_t>(archive.GetEntryCount());
    const std::size_t totalEntries = CountPlannedEntries(request.ZipPath, &progressSink, stopToken);
    std::size_t processedEntries = 0;
    std::size_t importedEntries = 0;
    std::size_t failedEntries = 0;
    std::size_t skippedEntries = 0;

    for (zip_uint64_t index = 0; index < entryCount; ++index)
    {
        if (stopToken.stop_requested() || progressSink.IsCancellationRequested())
        {
            result.Entries.push_back({
                .ArchivePath = {},
                .Status = EZipEntryImportStatus::Cancelled
            });
            break;
        }

        const std::string entryName = archive.GetEntryName(index);
        const std::filesystem::path entryPath = entryName;

        if (IsDirectoryEntry(entryName))
        {
            continue;
        }

        if (IsNestedArchive(entryPath))
        {
            result.Entries.push_back({
                .ArchivePath = entryPath,
                .Status = EZipEntryImportStatus::NestedArchiveSkipped,
                .Error = "Nested ZIP archives are not supported."
            });
            LogZipEntryIssueIfInitialized(
                request.ZipPath,
                entryPath,
                "zip-entry-filter",
                "skipped",
                "nested-archive-skipped",
                result.Entries.back().Error);
            ++processedEntries;
            ++skippedEntries;
            if (auto* structuredSink = dynamic_cast<Librova::Domain::IStructuredImportProgressSink*>(&progressSink); structuredSink != nullptr)
            {
                const auto percent = static_cast<int>((processedEntries * 100) / std::max<std::size_t>(totalEntries, 1));
                structuredSink->ReportStructuredProgress(
                    totalEntries,
                    processedEntries,
                    importedEntries,
                    failedEntries,
                    skippedEntries,
                    percent,
                    "Skipping nested ZIP entry");
            }
            continue;
        }

        if (!IsSafeRelativeEntryPath(entryPath.lexically_normal()))
        {
            result.Entries.push_back({
                .ArchivePath = entryPath,
                .Status = EZipEntryImportStatus::UnsupportedEntry,
                .Error = "Unsafe ZIP entry path."
            });
            LogZipEntryIssueIfInitialized(
                request.ZipPath,
                entryPath,
                "zip-entry-filter",
                "skipped",
                "unsafe-entry-path",
                result.Entries.back().Error);
            ++processedEntries;
            ++skippedEntries;
            if (auto* structuredSink = dynamic_cast<Librova::Domain::IStructuredImportProgressSink*>(&progressSink); structuredSink != nullptr)
            {
                const auto percent = static_cast<int>((processedEntries * 100) / std::max<std::size_t>(totalEntries, 1));
                structuredSink->ReportStructuredProgress(
                    totalEntries,
                    processedEntries,
                    importedEntries,
                    failedEntries,
                    skippedEntries,
                    percent,
                    "Skipping unsafe ZIP entry");
            }
            continue;
        }

        if (!IsSupportedBookEntry(entryPath))
        {
            result.Entries.push_back({
                .ArchivePath = entryPath,
                .Status = EZipEntryImportStatus::UnsupportedEntry,
                .Error = "Unsupported ZIP entry format."
            });
            LogZipEntryIssueIfInitialized(
                request.ZipPath,
                entryPath,
                "zip-entry-filter",
                "skipped",
                "unsupported-entry-format",
                result.Entries.back().Error);
            ++processedEntries;
            ++skippedEntries;
            if (auto* structuredSink = dynamic_cast<Librova::Domain::IStructuredImportProgressSink*>(&progressSink); structuredSink != nullptr)
            {
                const auto percent = static_cast<int>((processedEntries * 100) / std::max<std::size_t>(totalEntries, 1));
                structuredSink->ReportStructuredProgress(
                    totalEntries,
                    processedEntries,
                    importedEntries,
                    failedEntries,
                    skippedEntries,
                    percent,
                    "Skipping unsupported ZIP entry");
            }
            continue;
        }

        const std::filesystem::path extractedPath =
            ExtractEntryToWorkspace(archive, index, entryName, request.WorkingDirectory);
        auto cleanupExtractedPath = [&]() noexcept
        {
            CleanupExtractedEntryNoThrow(request.WorkingDirectory, extractedPath);
        };
        CScopedStructuredProgressSink entryProgressSink(progressSink, totalEntries, processedEntries, 1);
        entryProgressSink.ReportValue(5, "Importing ZIP entry");

        const std::filesystem::path entryWorkingDirectory = request.WorkingDirectory / "entries" / entryPath.stem();
        const auto cleanupEntryWorkspace = [&]() noexcept
        {
            CleanupEntryWorkspaceNoThrow(request.WorkingDirectory, entryWorkingDirectory);
        };
        const auto singleFileResult = [&]() {
            try
            {
                return m_singleFileImporter.Run({
                    .SourcePath = extractedPath,
                    .WorkingDirectory = entryWorkingDirectory,
                    .AllowProbableDuplicates = request.AllowProbableDuplicates,
                    .ForceEpubConversion = request.ForceEpubConversion
                }, entryProgressSink, stopToken);
            }
            catch (...)
            {
                cleanupExtractedPath();
                cleanupEntryWorkspace();
                throw;
            }
        }();
        cleanupExtractedPath();
        cleanupEntryWorkspace();

        result.Entries.push_back({
            .ArchivePath = entryPath,
            .Status = singleFileResult.IsSuccess()
                ? EZipEntryImportStatus::Imported
                : (singleFileResult.Status == Librova::Importing::ESingleFileImportStatus::Cancelled
                    ? EZipEntryImportStatus::Cancelled
                    : (IsSkippedSingleFileResult(singleFileResult)
                        ? EZipEntryImportStatus::Skipped
                        : EZipEntryImportStatus::Failed)),
            .SingleFileResult = std::move(singleFileResult)
        });

        ++processedEntries;
        if (result.Entries.back().Status == EZipEntryImportStatus::Imported)
        {
            ++importedEntries;
        }
        else if (result.Entries.back().Status == EZipEntryImportStatus::Failed)
        {
            ++failedEntries;
            LogZipEntryIssueIfInitialized(
                request.ZipPath,
                entryPath,
                GetSingleFileStage(singleFileResult),
                "failed",
                GetSingleFileStatus(singleFileResult),
                GetSingleFileLogReason(singleFileResult));
        }
        else if (result.Entries.back().Status == EZipEntryImportStatus::Skipped)
        {
            ++skippedEntries;
            LogZipEntryIssueIfInitialized(
                request.ZipPath,
                entryPath,
                GetSingleFileStage(singleFileResult),
                "skipped",
                GetSingleFileStatus(singleFileResult),
                GetSingleFileLogReason(singleFileResult));
        }

        if (auto* structuredSink = dynamic_cast<Librova::Domain::IStructuredImportProgressSink*>(&progressSink); structuredSink != nullptr)
        {
            const auto percent = static_cast<int>((processedEntries * 100) / std::max<std::size_t>(totalEntries, 1));
            structuredSink->ReportStructuredProgress(
                totalEntries,
                processedEntries,
                importedEntries,
                failedEntries,
                skippedEntries,
                percent,
                "Processed ZIP entry");
        }
    }

    return result;
}

} // namespace Librova::ZipImporting
