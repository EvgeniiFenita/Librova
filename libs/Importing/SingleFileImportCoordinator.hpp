#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include "Domain/BookRepository.hpp"
#include "Domain/ServiceContracts.hpp"
#include "ImportConversion/ImportConversionPolicy.hpp"
#include "Importing/ImportPerfTracker.hpp"
#include "ParserRegistry/BookParserRegistry.hpp"

namespace Librova::Importing {

enum class ESingleFileImportStatus
{
    Imported,
    RejectedDuplicate,
    Cancelled,
    Failed,
    UnsupportedFormat
};

struct SSingleFileImportRequest
{
    std::filesystem::path SourcePath;
    std::filesystem::path WorkingDirectory;
    std::optional<std::string> Sha256Hex;
    std::optional<Librova::Domain::SBookId> PreReservedBookId;
    std::optional<std::string> LogicalSourceLabel;
    std::uint64_t ImportJobId = 0;
    bool AllowProbableDuplicates = false;
    bool ForceEpubConversion = false;
    std::optional<std::reference_wrapper<CImportPerfTracker>> PerfTracker; // optional; pass std::ref(perf) for per-stage measurements
    std::optional<std::reference_wrapper<Librova::Domain::IBookRepository>> RepositoryOverride; // optional; overrides m_bookRepository for writes

    [[nodiscard]] bool IsValid() const noexcept
    {
        return !SourcePath.empty() && !WorkingDirectory.empty();
    }
};

struct SSingleFileImportResult
{
    ESingleFileImportStatus Status = ESingleFileImportStatus::Failed;
    std::optional<Librova::Domain::SBookId> ImportedBookId;
    std::optional<Librova::Domain::EBookFormat> StoredFormat;
    std::vector<Librova::Domain::SDuplicateMatch> DuplicateMatches;
    std::vector<std::string> Warnings;
    std::string Error;
    std::string DiagnosticError;

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
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token stopToken) const = 0;
};

class CSingleFileImportCoordinator final : public ISingleFileImporter
{
public:
    CSingleFileImportCoordinator(
        const Librova::ParserRegistry::CBookParserRegistry& parserRegistry,
        Librova::Domain::IBookRepository& bookRepository,
        const Librova::Domain::IBookQueryRepository& queryRepository,
        Librova::Domain::IManagedStorage& managedStorage,
        const Librova::Domain::IBookConverter* converter,
        const Librova::Domain::ICoverImageProcessor* coverImageProcessor = nullptr);

    [[nodiscard]] SSingleFileImportResult Run(
        const SSingleFileImportRequest& request,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token stopToken) const override;

private:
    [[nodiscard]] static Librova::Domain::SCandidateBook BuildCandidateBook(
        const Librova::Domain::SParsedBook& parsedBook,
        const std::optional<std::string>& sha256Hex);

    const Librova::ParserRegistry::CBookParserRegistry& m_parserRegistry;
    Librova::Domain::IBookRepository& m_bookRepository;
    const Librova::Domain::IBookQueryRepository& m_queryRepository;
    Librova::Domain::IManagedStorage& m_managedStorage;
    const Librova::Domain::IBookConverter* m_converter;
    const Librova::Domain::ICoverImageProcessor* m_coverImageProcessor;
};

} // namespace Librova::Importing
