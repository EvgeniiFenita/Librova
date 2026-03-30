#include "ZipImporting/ZipImportCoordinator.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <zip.h>

namespace {

class CZipArchive final
{
public:
    explicit CZipArchive(const std::filesystem::path& filePath)
    {
        int errorCode = ZIP_ER_OK;
        m_archive = zip_open(filePath.string().c_str(), ZIP_RDONLY, &errorCode);

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

void EnsureDirectory(const std::filesystem::path& path)
{
    std::error_code errorCode;
    std::filesystem::create_directories(path, errorCode);

    if (errorCode)
    {
        throw std::runtime_error(std::string{"Failed to create directory: "} + path.string());
    }
}

std::filesystem::path ExtractEntryToWorkspace(
    const CZipArchive& archive,
    const zip_uint64_t index,
    const std::string_view entryName,
    const std::filesystem::path& workingDirectory)
{
    const std::filesystem::path relativePath = std::filesystem::path{entryName}.lexically_normal();
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

} // namespace

namespace LibriFlow::ZipImporting {

std::size_t SZipImportResult::ImportedCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(Entries.begin(), Entries.end(), [](const auto& entry) {
        return entry.IsImported();
    }));
}

CZipImportCoordinator::CZipImportCoordinator(const LibriFlow::Importing::ISingleFileImporter& singleFileImporter)
    : m_singleFileImporter(singleFileImporter)
{
}

SZipImportResult CZipImportCoordinator::Run(
    const SZipImportRequest& request,
    LibriFlow::Domain::IProgressSink& progressSink,
    const std::stop_token stopToken) const
{
    if (!request.IsValid())
    {
        throw std::invalid_argument("ZIP import request must contain archive path and working directory.");
    }

    CZipArchive archive(request.ZipPath);
    SZipImportResult result;
    const zip_uint64_t entryCount = static_cast<zip_uint64_t>(archive.GetEntryCount());

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
            continue;
        }

        if (!IsSupportedBookEntry(entryPath))
        {
            result.Entries.push_back({
                .ArchivePath = entryPath,
                .Status = EZipEntryImportStatus::UnsupportedEntry
            });
            continue;
        }

        const std::filesystem::path extractedPath =
            ExtractEntryToWorkspace(archive, index, entryName, request.WorkingDirectory);
        progressSink.ReportValue(
            static_cast<int>(((index + 1) * 100) / std::max<zip_uint64_t>(entryCount, 1)),
            "Importing ZIP entry");

        const auto singleFileResult = m_singleFileImporter.Run({
            .SourcePath = extractedPath,
            .WorkingDirectory = request.WorkingDirectory / "entries" / entryPath.stem(),
            .AllowProbableDuplicates = request.AllowProbableDuplicates
        }, progressSink, stopToken);

        result.Entries.push_back({
            .ArchivePath = entryPath,
            .Status = singleFileResult.IsSuccess() ? EZipEntryImportStatus::Imported : EZipEntryImportStatus::Failed,
            .SingleFileResult = std::move(singleFileResult)
        });
    }

    return result;
}

} // namespace LibriFlow::ZipImporting
