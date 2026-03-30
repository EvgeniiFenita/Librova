#pragma once

#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include "Domain/BookRepository.hpp"
#include "Domain/ServiceContracts.hpp"
#include "ImportConversion/ImportConversionPolicy.hpp"
#include "ParserRegistry/BookParserRegistry.hpp"

namespace LibriFlow::Importing {

enum class ESingleFileImportStatus
{
    Imported,
    RejectedDuplicate,
    DecisionRequired,
    Cancelled,
    Failed,
    UnsupportedFormat
};

struct SSingleFileImportRequest
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

struct SSingleFileImportResult
{
    ESingleFileImportStatus Status = ESingleFileImportStatus::Failed;
    std::optional<LibriFlow::Domain::SBookId> ImportedBookId;
    std::optional<LibriFlow::Domain::EBookFormat> StoredFormat;
    std::vector<LibriFlow::Domain::SDuplicateMatch> DuplicateMatches;
    std::vector<std::string> Warnings;
    std::string Error;

    [[nodiscard]] bool IsSuccess() const noexcept
    {
        return Status == ESingleFileImportStatus::Imported && ImportedBookId.has_value();
    }
};

class ISingleFileImporter
{
public:
    virtual ~ISingleFileImporter() = default;

    [[nodiscard]] virtual SSingleFileImportResult Run(
        const SSingleFileImportRequest& request,
        LibriFlow::Domain::IProgressSink& progressSink,
        std::stop_token stopToken) const = 0;
};

class CSingleFileImportCoordinator final : public ISingleFileImporter
{
public:
    CSingleFileImportCoordinator(
        const LibriFlow::ParserRegistry::CBookParserRegistry& parserRegistry,
        LibriFlow::Domain::IBookRepository& bookRepository,
        const LibriFlow::Domain::IBookQueryRepository& queryRepository,
        LibriFlow::Domain::IManagedStorage& managedStorage,
        const LibriFlow::Domain::IBookConverter* converter);

    [[nodiscard]] SSingleFileImportResult Run(
        const SSingleFileImportRequest& request,
        LibriFlow::Domain::IProgressSink& progressSink,
        std::stop_token stopToken) const override;

private:
    [[nodiscard]] static LibriFlow::Domain::SCandidateBook BuildCandidateBook(
        const LibriFlow::Domain::SParsedBook& parsedBook,
        const std::optional<std::string>& sha256Hex);

    const LibriFlow::ParserRegistry::CBookParserRegistry& m_parserRegistry;
    LibriFlow::Domain::IBookRepository& m_bookRepository;
    const LibriFlow::Domain::IBookQueryRepository& m_queryRepository;
    LibriFlow::Domain::IManagedStorage& m_managedStorage;
    const LibriFlow::Domain::IBookConverter* m_converter;
};

} // namespace LibriFlow::Importing
