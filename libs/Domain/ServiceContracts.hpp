#pragma once

#include <filesystem>
#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include "Domain/Book.hpp"
#include "Domain/CandidateBook.hpp"

namespace Librova::Domain {

struct SParsedBook
{
    SBookMetadata Metadata;
    EBookFormat SourceFormat = EBookFormat::Epub;
    std::optional<std::string> CoverExtension;
    std::vector<std::byte> CoverBytes;
    std::optional<std::string> CoverDiagnosticMessage;

    [[nodiscard]] bool HasCover() const noexcept
    {
        return CoverExtension.has_value() && !CoverExtension->empty() && !CoverBytes.empty();
    }
};

struct SBookParseOptions
{
    bool ExtractCover = true;
};

struct SConversionRequest
{
    std::filesystem::path SourcePath;
    std::filesystem::path DestinationPath;
    EBookFormat SourceFormat = EBookFormat::Fb2;
    EBookFormat DestinationFormat = EBookFormat::Epub;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return !SourcePath.empty() && !DestinationPath.empty() && SourceFormat != DestinationFormat;
    }
};

enum class EConversionStatus
{
    Failed,
    Succeeded,
    Cancelled
};

struct SConversionResult
{
    EConversionStatus Status = EConversionStatus::Failed;
    std::filesystem::path OutputPath;
    std::vector<std::string> Warnings;

    [[nodiscard]] bool IsSuccess() const noexcept
    {
        return Status == EConversionStatus::Succeeded;
    }

    [[nodiscard]] bool IsCancelled() const noexcept
    {
        return Status == EConversionStatus::Cancelled;
    }

    [[nodiscard]] bool HasOutput() const noexcept
    {
        return !OutputPath.empty();
    }
};

struct SCoverData
{
    std::string Extension;
    std::vector<std::byte> Bytes;

    [[nodiscard]] bool IsEmpty() const noexcept
    {
        return Extension.empty() || Bytes.empty();
    }
};

enum class ECoverProcessingStatus
{
    Processed,
    Unchanged,
    Unsupported,
    Failed
};

struct SCoverProcessingRequest
{
    SCoverData Cover;
    std::uint32_t MaxWidth = 0;
    std::uint32_t MaxHeight = 0;
    std::uint32_t FallbackMaxWidth = 0;
    std::uint32_t FallbackMaxHeight = 0;
    std::uint32_t TargetMaxBytes = 0;
    bool PreserveSmallerImages = true;
    bool AllowFormatConversion = false;

    [[nodiscard]] bool IsValid() const noexcept
    {
        const bool hasValidPrimaryBounds = MaxWidth > 0 && MaxHeight > 0;
        const bool hasNoFallbackBounds = FallbackMaxWidth == 0 && FallbackMaxHeight == 0;
        const bool hasValidFallbackBounds = FallbackMaxWidth > 0 && FallbackMaxHeight > 0;
        return !Cover.IsEmpty() && hasValidPrimaryBounds && (hasNoFallbackBounds || hasValidFallbackBounds);
    }
};

struct SCoverProcessingResult
{
    ECoverProcessingStatus Status = ECoverProcessingStatus::Failed;
    SCoverData Cover;
    std::uint32_t PixelWidth = 0;
    std::uint32_t PixelHeight = 0;
    bool WasResized = false;
    std::string DiagnosticMessage;

    [[nodiscard]] bool HasOutputCover() const noexcept
    {
        return !Cover.IsEmpty();
    }
};

struct SStoragePlan
{
    SBookId BookId;
    EBookFormat Format = EBookFormat::Epub;
    EStorageEncoding StorageEncoding = EStorageEncoding::Plain;
    std::filesystem::path SourcePath;
    std::optional<std::filesystem::path> CoverSourcePath;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return BookId.IsValid() && !SourcePath.empty();
    }
};

struct SPreparedStorage
{
    std::filesystem::path StagedBookPath;
    std::optional<std::filesystem::path> StagedCoverPath;
    std::filesystem::path FinalBookPath;
    std::optional<std::filesystem::path> FinalCoverPath;
    // Paths relative to the library root — stored in the database.
    std::filesystem::path RelativeBookPath;
    std::optional<std::filesystem::path> RelativeCoverPath;

    [[nodiscard]] bool HasStagedBook() const noexcept
    {
        return !StagedBookPath.empty();
    }
};

class IProgressSink
{
public:
    virtual ~IProgressSink() = default;

    virtual void ReportValue(int percent, std::string_view message) = 0;
    virtual bool IsCancellationRequested() const = 0;

    // Post-cancel lifecycle hooks. Called by the import facade to report
    // rollback and compaction phases. Default implementations are no-ops so
    // existing IProgressSink consumers require no changes.
    virtual void BeginRollback(std::size_t totalToRollback) noexcept {}
    virtual void ReportRollbackProgress(std::size_t rolledBack, std::size_t total) noexcept {}
    virtual void BeginCompacting() noexcept {}
};

class IStructuredImportProgressSink
{
public:
    virtual ~IStructuredImportProgressSink() = default;

    virtual void ReportStructuredProgress(
        std::size_t totalEntries,
        std::size_t processedEntries,
        std::size_t importedEntries,
        std::size_t failedEntries,
        std::size_t skippedEntries,
        int percent,
        std::string_view message) = 0;
};

class IBookParser
{
public:
    virtual ~IBookParser() = default;

    virtual bool CanParse(EBookFormat format) const = 0;
    virtual SParsedBook Parse(
        const std::filesystem::path& filePath,
        std::string_view logicalSourceLabel = {},
        const SBookParseOptions& options = {}) const = 0;
};

class IBookConverter
{
public:
    virtual ~IBookConverter() = default;

    virtual bool CanConvert(EBookFormat sourceFormat, EBookFormat destinationFormat) const = 0;
    virtual SConversionResult Convert(
        const SConversionRequest& request,
        IProgressSink& progressSink,
        std::stop_token stopToken) const = 0;
};

class IManagedStorage
{
public:
    virtual ~IManagedStorage() = default;

    virtual SPreparedStorage PrepareImport(const SStoragePlan& plan) = 0;
    virtual void CommitImport(const SPreparedStorage& preparedStorage) = 0;
    virtual void RollbackImport(const SPreparedStorage& preparedStorage) noexcept = 0;
};

class ITrashService
{
public:
    virtual ~ITrashService() = default;

    [[nodiscard]] virtual std::filesystem::path MoveToTrash(const std::filesystem::path& path) = 0;
    virtual void RestoreFromTrash(
        const std::filesystem::path& trashedPath,
        const std::filesystem::path& destinationPath) = 0;
};

class IRecycleBinService
{
public:
    virtual ~IRecycleBinService() = default;

    virtual void MoveToRecycleBin(const std::vector<std::filesystem::path>& paths) = 0;
};

class ICoverProvider
{
public:
    virtual ~ICoverProvider() = default;

    virtual std::optional<SCoverData> TryResolve(const SBookMetadata& metadata) const = 0;
};

class ICoverImageProcessor
{
public:
    virtual ~ICoverImageProcessor() = default;

    [[nodiscard]] virtual SCoverProcessingResult ProcessForManagedStorage(const SCoverProcessingRequest& request) const = 0;
};

} // namespace Librova::Domain
