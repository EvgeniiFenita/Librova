#pragma once

#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
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

    [[nodiscard]] bool HasCover() const noexcept
    {
        return CoverExtension.has_value() && !CoverExtension->empty() && !CoverBytes.empty();
    }
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

struct SStoragePlan
{
    SBookId BookId;
    EBookFormat Format = EBookFormat::Epub;
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
};

class IBookParser
{
public:
    virtual ~IBookParser() = default;

    virtual bool CanParse(EBookFormat format) const = 0;
    virtual SParsedBook Parse(const std::filesystem::path& filePath) const = 0;
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

class ICoverProvider
{
public:
    virtual ~ICoverProvider() = default;

    virtual std::optional<SCoverData> TryResolve(const SBookMetadata& metadata) const = 0;
};

} // namespace Librova::Domain
