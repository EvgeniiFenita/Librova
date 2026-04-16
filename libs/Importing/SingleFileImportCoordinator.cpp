#include "Importing/SingleFileImportCoordinator.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "Domain/DuplicateMatch.hpp"

#include "Hashing/Sha256.hpp"
#include "Logging/Logging.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace {

constexpr auto GStrictDuplicateWarning = "Import rejected because a strict duplicate already exists.";
constexpr auto GProbableDuplicateWarning = "Import skipped because a probable duplicate was found.";
constexpr auto GForcedStrictDuplicateWarning = "Import continued with explicit strict duplicate override.";
constexpr auto GForcedProbableDuplicateWarning = "Import continued with explicit probable duplicate override.";

[[nodiscard]] std::string_view DuplicateReasonLabel(const Librova::Domain::EDuplicateReason reason) noexcept
{
    switch (reason)
    {
    case Librova::Domain::EDuplicateReason::SameHash:
        return "SameHash";
    case Librova::Domain::EDuplicateReason::SameIsbn:
        return "SameIsbn";
    case Librova::Domain::EDuplicateReason::SameNormalizedTitleAndAuthors:
        return "SameNormalizedTitleAndAuthors";
    }
    return "Unknown";
}

[[nodiscard]] std::string_view DuplicateSeverityLabel(const Librova::Domain::EDuplicateSeverity severity) noexcept
{
    switch (severity)
    {
    case Librova::Domain::EDuplicateSeverity::Strict:
        return "Strict";
    case Librova::Domain::EDuplicateSeverity::Probable:
        return "Probable";
    case Librova::Domain::EDuplicateSeverity::None:
        return "None";
    }
    return "Unknown";
}

[[nodiscard]] std::string JoinAuthors(const std::vector<std::string>& authors)
{
    std::string result;
    for (std::size_t i = 0; i < authors.size(); ++i)
    {
        if (i > 0)
            result += "; ";
        result += authors[i];
    }
    return result;
}

[[nodiscard]] std::string GetLogicalSourceLabel(const Librova::Importing::SSingleFileImportRequest& request)
{
    return request.LogicalSourceLabel.value_or(Librova::Unicode::PathToUtf8(request.SourcePath));
}

[[nodiscard]] const Librova::Domain::SDuplicateMatch* FindDecisiveDuplicateMatch(
    const std::vector<Librova::Domain::SDuplicateMatch>& matches) noexcept
{
    const auto strictMatch = std::find_if(matches.begin(), matches.end(), [](const auto& match) {
        return match.Severity == Librova::Domain::EDuplicateSeverity::Strict;
    });
    if (strictMatch != matches.end())
    {
        return &*strictMatch;
    }

    const auto probableMatch = std::find_if(matches.begin(), matches.end(), [](const auto& match) {
        return match.Severity == Librova::Domain::EDuplicateSeverity::Probable;
    });
    return probableMatch != matches.end() ? &*probableMatch : nullptr;
}

void LogDuplicateCandidateDetails(
    const Librova::Importing::SSingleFileImportRequest& request,
    const Librova::Domain::SBookMetadata& metadata,
    const std::optional<std::string>& sha256Hex,
    const std::vector<Librova::Domain::SDuplicateMatch>& matches,
    const bool warningLevel)
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    const std::string authorsStr = JoinAuthors(metadata.AuthorsUtf8);
    const std::string sourceLabel = GetLogicalSourceLabel(request);

    for (const auto& match : matches)
    {
        std::string matchDetail;
        switch (match.Reason)
        {
        case Librova::Domain::EDuplicateReason::SameHash:
            matchDetail = sha256Hex.value_or("<unavailable>");
            break;
        case Librova::Domain::EDuplicateReason::SameIsbn:
            matchDetail = metadata.Isbn.value_or("<unavailable>");
            break;
        case Librova::Domain::EDuplicateReason::SameNormalizedTitleAndAuthors:
            matchDetail = "title='" + metadata.TitleUtf8 + "' authors='" + authorsStr + "'";
            break;
        }

        if (warningLevel)
        {
            if (request.ImportJobId != 0)
            {
                Librova::Logging::Warn(
                    "Duplicate candidate fields: job={} source='{}' title='{}' authors='{}' severity='{}' reason='{}' matchValue='{}' existingBookId={} existingTitle='{}' existingAuthors='{}'",
                    request.ImportJobId,
                    sourceLabel,
                    metadata.TitleUtf8,
                    authorsStr,
                    DuplicateSeverityLabel(match.Severity),
                    DuplicateReasonLabel(match.Reason),
                    matchDetail,
                    match.ExistingBookId.Value,
                    match.ExistingTitle,
                    match.ExistingAuthors);
            }
            else
            {
                Librova::Logging::Warn(
                    "Duplicate candidate fields: source='{}' title='{}' authors='{}' severity='{}' reason='{}' matchValue='{}' existingBookId={} existingTitle='{}' existingAuthors='{}'",
                    sourceLabel,
                    metadata.TitleUtf8,
                    authorsStr,
                    DuplicateSeverityLabel(match.Severity),
                    DuplicateReasonLabel(match.Reason),
                    matchDetail,
                    match.ExistingBookId.Value,
                    match.ExistingTitle,
                    match.ExistingAuthors);
            }
        }
        else
        {
            if (request.ImportJobId != 0)
            {
                Librova::Logging::Info(
                    "Duplicate candidate fields: job={} source='{}' title='{}' authors='{}' severity='{}' reason='{}' matchValue='{}' existingBookId={} existingTitle='{}' existingAuthors='{}'",
                    request.ImportJobId,
                    sourceLabel,
                    metadata.TitleUtf8,
                    authorsStr,
                    DuplicateSeverityLabel(match.Severity),
                    DuplicateReasonLabel(match.Reason),
                    matchDetail,
                    match.ExistingBookId.Value,
                    match.ExistingTitle,
                    match.ExistingAuthors);
            }
            else
            {
                Librova::Logging::Info(
                    "Duplicate candidate fields: source='{}' title='{}' authors='{}' severity='{}' reason='{}' matchValue='{}' existingBookId={} existingTitle='{}' existingAuthors='{}'",
                    sourceLabel,
                    metadata.TitleUtf8,
                    authorsStr,
                    DuplicateSeverityLabel(match.Severity),
                    DuplicateReasonLabel(match.Reason),
                    matchDetail,
                    match.ExistingBookId.Value,
                    match.ExistingTitle,
                    match.ExistingAuthors);
            }
        }
    }
}

void LogDuplicateDecision(
    const Librova::Importing::SSingleFileImportRequest& request,
    const std::vector<Librova::Domain::SDuplicateMatch>& matches,
    std::string_view outcome,
    const bool warningLevel)
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    const auto* decisiveMatch = FindDecisiveDuplicateMatch(matches);
    if (decisiveMatch == nullptr)
    {
        return;
    }

    const std::string sourceLabel = GetLogicalSourceLabel(request);
    const auto additionalMatches = matches.size() > 0 ? matches.size() - 1 : 0;

    if (warningLevel)
    {
        if (request.ImportJobId != 0)
        {
            Librova::Logging::Warn(
                "duplicate: decision job={} source='{}' outcome='{}' decisiveSeverity='{}' decisiveReason='{}' decisiveExistingBookId={} additionalMatches={}",
                request.ImportJobId,
                sourceLabel,
                outcome,
                DuplicateSeverityLabel(decisiveMatch->Severity),
                DuplicateReasonLabel(decisiveMatch->Reason),
                decisiveMatch->ExistingBookId.Value,
                additionalMatches);
        }
        else
        {
            Librova::Logging::Warn(
                "duplicate: decision source='{}' outcome='{}' decisiveSeverity='{}' decisiveReason='{}' decisiveExistingBookId={} additionalMatches={}",
                sourceLabel,
                outcome,
                DuplicateSeverityLabel(decisiveMatch->Severity),
                DuplicateReasonLabel(decisiveMatch->Reason),
                decisiveMatch->ExistingBookId.Value,
                additionalMatches);
        }
    }
    else
    {
        if (request.ImportJobId != 0)
        {
            Librova::Logging::Info(
                "duplicate: decision job={} source='{}' outcome='{}' decisiveSeverity='{}' decisiveReason='{}' decisiveExistingBookId={} additionalMatches={}",
                request.ImportJobId,
                sourceLabel,
                outcome,
                DuplicateSeverityLabel(decisiveMatch->Severity),
                DuplicateReasonLabel(decisiveMatch->Reason),
                decisiveMatch->ExistingBookId.Value,
                additionalMatches);
        }
        else
        {
            Librova::Logging::Info(
                "duplicate: decision source='{}' outcome='{}' decisiveSeverity='{}' decisiveReason='{}' decisiveExistingBookId={} additionalMatches={}",
                sourceLabel,
                outcome,
                DuplicateSeverityLabel(decisiveMatch->Severity),
                DuplicateReasonLabel(decisiveMatch->Reason),
                decisiveMatch->ExistingBookId.Value,
                additionalMatches);
        }
    }
}

void LogCoverOutcome(
    const Librova::Importing::SSingleFileImportRequest& request,
    std::string_view outcome,
    std::string_view reason,
    const std::string_view extension = "<none>",
    const std::size_t bytes = 0,
    const bool warningLevel = false)
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    const std::string sourceLabel = GetLogicalSourceLabel(request);
    if (warningLevel)
    {
        if (request.ImportJobId != 0)
        {
            Librova::Logging::Warn(
                "cover: final job={} source='{}' outcome='{}' reason='{}' ext='{}' bytes={}",
                request.ImportJobId,
                sourceLabel,
                outcome,
                reason,
                extension,
                bytes);
        }
        else
        {
            Librova::Logging::Warn(
                "cover: final source='{}' outcome='{}' reason='{}' ext='{}' bytes={}",
                sourceLabel,
                outcome,
                reason,
                extension,
                bytes);
        }
        return;
    }

    if (request.ImportJobId != 0)
    {
        Librova::Logging::Info(
            "cover: final job={} source='{}' outcome='{}' reason='{}' ext='{}' bytes={}",
            request.ImportJobId,
            sourceLabel,
            outcome,
            reason,
            extension,
            bytes);
    }
    else
    {
        Librova::Logging::Info(
            "cover: final source='{}' outcome='{}' reason='{}' ext='{}' bytes={}",
            sourceLabel,
            outcome,
            reason,
            extension,
            bytes);
    }
}

[[nodiscard]] std::string_view ConversionStatusLabel(const Librova::Domain::EConversionStatus status) noexcept
{
    switch (status)
    {
    case Librova::Domain::EConversionStatus::Failed:
        return "failed";
    case Librova::Domain::EConversionStatus::Succeeded:
        return "succeeded";
    case Librova::Domain::EConversionStatus::Cancelled:
        return "cancelled";
    }

    return "failed";
}

void LogConversionOutcome(
    const Librova::Importing::SSingleFileImportRequest& request,
    const Librova::Domain::SConversionResult& result)
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    const std::string sourceLabel = GetLogicalSourceLabel(request);
    const auto warningText = result.Warnings.empty() ? "<none>" : result.Warnings.front();
    const auto outputPath = result.HasOutput() ? Librova::Unicode::PathToUtf8(result.OutputPath) : std::string{"<none>"};

    if (request.ImportJobId != 0)
    {
        Librova::Logging::Info(
            "convert: done job={} source='{}' outcome='{}' warnings={} firstWarning='{}' output='{}'",
            request.ImportJobId,
            sourceLabel,
            ConversionStatusLabel(result.Status),
            result.Warnings.size(),
            warningText,
            outputPath);
    }
    else
    {
        Librova::Logging::Info(
            "convert: done source='{}' outcome='{}' warnings={} firstWarning='{}' output='{}'",
            sourceLabel,
            ConversionStatusLabel(result.Status),
            result.Warnings.size(),
            warningText,
            outputPath);
    }
}

constexpr std::uint32_t GManagedCoverMaxWidth = 512;
constexpr std::uint32_t GManagedCoverMaxHeight = 768;

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

std::filesystem::path WriteCoverTempFile(
    const std::filesystem::path& workingDirectory,
    const Librova::Domain::SBookId bookId,
    const std::string& extension,
    const std::vector<std::byte>& bytes)
{
    const std::filesystem::path tempDirectory = workingDirectory / "covers";
    EnsureDirectory(tempDirectory);

    const std::filesystem::path coverPath =
        tempDirectory / std::filesystem::path{"cover-" + std::to_string(bookId.Value)}.replace_extension(extension);

    std::ofstream output(coverPath, std::ios::binary);

    if (!output)
    {
        throw std::runtime_error("Failed to create temporary cover file.");
    }

    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return coverPath;
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

bool HasStrictDuplicate(const std::vector<Librova::Domain::SDuplicateMatch>& duplicates)
{
    for (const auto& duplicate : duplicates)
    {
        if (duplicate.IsStrictRejection())
        {
            return true;
        }
    }

    return false;
}

bool HasProbableDuplicate(const std::vector<Librova::Domain::SDuplicateMatch>& duplicates)
{
    for (const auto& duplicate : duplicates)
    {
        if (duplicate.IsProbable())
        {
            return true;
        }
    }

    return false;
}

bool IsCancellationRequested(
    const std::stop_token stopToken,
    const Librova::Domain::IProgressSink& progressSink)
{
    return stopToken.stop_requested() || progressSink.IsCancellationRequested();
}

[[nodiscard]] std::string SanitizeTransportErrorMessage(const std::string_view message)
{
    constexpr std::string_view detailedAuthorMessage =
        "FB2 metadata must contain at least one non-empty title-info/author.";

    if (message.starts_with(detailedAuthorMessage))
    {
        return std::string{detailedAuthorMessage};
    }

    if (const auto diagnosticsIndex = message.find(" ["); diagnosticsIndex != std::string_view::npos)
    {
        return std::string{message.substr(0, diagnosticsIndex)};
    }

    return std::string{message};
}

[[nodiscard]] Librova::Domain::SCoverData BuildSourceCoverData(const Librova::Domain::SParsedBook& parsedBook)
{
    return {
        .Extension = *parsedBook.CoverExtension,
        .Bytes = parsedBook.CoverBytes
    };
}

struct SResolvedManagedCoverData
{
    Librova::Domain::SCoverData Cover;
    std::string FinalReason;
};

[[nodiscard]] SResolvedManagedCoverData ResolveManagedCoverData(
    const Librova::Importing::SSingleFileImportRequest& request,
    const Librova::Domain::SParsedBook& parsedBook,
    const Librova::Domain::ICoverImageProcessor* coverImageProcessor)
{
    const auto sourceCover = BuildSourceCoverData(parsedBook);
    const std::string sourceLabel = GetLogicalSourceLabel(request);

    if (coverImageProcessor == nullptr)
    {
        return {
            .Cover = sourceCover,
            .FinalReason = "stored-original-no-processor"
        };
    }

    const auto processingResult = coverImageProcessor->ProcessForManagedStorage({
        .Cover = sourceCover,
        .MaxWidth = GManagedCoverMaxWidth,
        .MaxHeight = GManagedCoverMaxHeight,
        .PreserveSmallerImages = true,
        .AllowFormatConversion = false
    });

    switch (processingResult.Status)
    {
    case Librova::Domain::ECoverProcessingStatus::Processed:
    case Librova::Domain::ECoverProcessingStatus::Unchanged:
        if (processingResult.HasOutputCover())
        {
            if (Librova::Logging::CLogging::IsInitialized())
            {
                Librova::Logging::Info(
                    "cover: stored source='{}' status='{}' ext='{}' dimensions={}x{} bytes={}",
                    sourceLabel,
                    processingResult.Status == Librova::Domain::ECoverProcessingStatus::Processed ? "processed" : "unchanged",
                    processingResult.Cover.Extension,
                    processingResult.PixelWidth,
                    processingResult.PixelHeight,
                    processingResult.Cover.Bytes.size());
            }

            return {
                .Cover = processingResult.Cover,
                .FinalReason = processingResult.Status == Librova::Domain::ECoverProcessingStatus::Processed
                    ? "stored-processed"
                    : "stored-unchanged"
            };
        }

        break;
    case Librova::Domain::ECoverProcessingStatus::Unsupported:
    case Librova::Domain::ECoverProcessingStatus::Failed:
        break;
    }

    if (Librova::Logging::CLogging::IsInitialized())
    {
        const std::string_view statusLabel = [&]() -> std::string_view {
            switch (processingResult.Status)
            {
            case Librova::Domain::ECoverProcessingStatus::Unsupported: return "unsupported";
            case Librova::Domain::ECoverProcessingStatus::Failed:      return "failed";
            case Librova::Domain::ECoverProcessingStatus::Processed:   return "processed-no-output";
            case Librova::Domain::ECoverProcessingStatus::Unchanged:   return "unchanged-no-output";
            }
            return "unknown";
        }();

        const std::string_view reason = processingResult.DiagnosticMessage.empty()
            ? std::string_view("(no diagnostic message from processor)")
            : std::string_view(processingResult.DiagnosticMessage);

        Librova::Logging::Warn(
            "cover: optimization skipped source='{}' status='{}' reason='{}' input_ext='{}' input_bytes={}",
            sourceLabel,
            statusLabel,
            reason,
            sourceCover.Extension,
            sourceCover.Bytes.size());
    }

    return {
        .Cover = sourceCover,
        .FinalReason = "stored-original-after-processor-fallback"
    };
}

[[nodiscard]] Librova::Domain::EStorageEncoding DetermineStorageEncoding(
    const Librova::Domain::EBookFormat storedFormat,
    const bool forceEpubConversion) noexcept
{
    if (storedFormat == Librova::Domain::EBookFormat::Fb2 && !forceEpubConversion)
    {
        return Librova::Domain::EStorageEncoding::Compressed;
    }

    return Librova::Domain::EStorageEncoding::Plain;
}

} // namespace

namespace Librova::Importing {

CSingleFileImportCoordinator::CSingleFileImportCoordinator(
    const Librova::ParserRegistry::CBookParserRegistry& parserRegistry,
    Librova::Domain::IBookRepository& bookRepository,
    const Librova::Domain::IBookQueryRepository& queryRepository,
    Librova::Domain::IManagedStorage& managedStorage,
    const Librova::Domain::IBookConverter* converter,
    const Librova::Domain::ICoverImageProcessor* coverImageProcessor)
    : m_parserRegistry(parserRegistry)
    , m_bookRepository(bookRepository)
    , m_queryRepository(queryRepository)
    , m_managedStorage(managedStorage)
    , m_converter(converter)
    , m_coverImageProcessor(coverImageProcessor)
{
}

Librova::Domain::SCandidateBook CSingleFileImportCoordinator::BuildCandidateBook(
    const Librova::Domain::SParsedBook& parsedBook,
    const std::optional<std::string>& sha256Hex)
{
    return {
        .Metadata = parsedBook.Metadata,
        .Format = parsedBook.SourceFormat,
        .Sha256Hex = sha256Hex
    };
}

SSingleFileImportResult CSingleFileImportCoordinator::Run(
    const SSingleFileImportRequest& request,
    Librova::Domain::IProgressSink& progressSink,
    const std::stop_token stopToken) const
{
    if (!request.IsValid())
    {
        return {
            .Status = ESingleFileImportStatus::Failed,
            .Error = "Import request must contain source path and working directory."
        };
    }

    std::optional<std::filesystem::path> temporaryCoverPath;
    std::optional<std::filesystem::path> temporaryConvertedPath;
    std::optional<Librova::Domain::SPreparedStorage> preparedStorage;
    std::optional<Librova::Domain::SBookId> addedBookId;
    const auto buildCancelledResult =
        [&](const std::optional<Librova::Domain::EBookFormat> storedFormat,
            const std::vector<Librova::Domain::SDuplicateMatch>& duplicates,
            std::vector<std::string> warnings) {
            if (preparedStorage.has_value())
            {
                m_managedStorage.RollbackImport(*preparedStorage);
            }

            RemovePathNoThrow(temporaryConvertedPath.value_or(std::filesystem::path{}));
            RemovePathNoThrow(temporaryCoverPath.value_or(std::filesystem::path{}));

            return SSingleFileImportResult{
                .Status = ESingleFileImportStatus::Cancelled,
                .StoredFormat = storedFormat,
                .DuplicateMatches = duplicates,
                .Warnings = std::move(warnings)
            };
        };
    const auto buildFailedResult =
        [&](std::string error,
            std::vector<std::string> warnings,
            std::string diagnosticError = {}) {
            if (preparedStorage.has_value())
            {
                m_managedStorage.RollbackImport(*preparedStorage);
            }

            RemovePathNoThrow(temporaryConvertedPath.value_or(std::filesystem::path{}));
            RemovePathNoThrow(temporaryCoverPath.value_or(std::filesystem::path{}));

            return SSingleFileImportResult{
                .Status = ESingleFileImportStatus::Failed,
                .Warnings = std::move(warnings),
                .Error = std::move(error),
                .DiagnosticError = std::move(diagnosticError)
            };
        };

    auto measureStage = [&](CImportPerfTracker::EStage stage) -> std::optional<CImportPerfTracker::CScopedStageTimer> {
        if (request.PerfTracker.has_value())
        {
            return request.PerfTracker->get().MeasureStage(stage);
        }

        return std::nullopt;
    };

    Librova::Domain::IBookRepository& repo =
        request.RepositoryOverride.has_value() ? request.RepositoryOverride->get() : m_bookRepository;
    const std::string sourceLabel = GetLogicalSourceLabel(request);

    try
    {
        if (IsCancellationRequested(stopToken, progressSink))
        {
            return buildCancelledResult(std::nullopt, {}, {});
        }

        progressSink.ReportValue(5, "Detecting source format");
        const std::optional<Librova::Domain::EBookFormat> detectedFormat =
            Librova::ParserRegistry::CBookParserRegistry::TryDetectFormat(request.SourcePath);

        if (!detectedFormat.has_value())
        {
            return {
                .Status = ESingleFileImportStatus::UnsupportedFormat,
                .Error = "Unsupported input format."
            };
        }

        progressSink.ReportValue(15, "Parsing source book");
        const Librova::Domain::SParsedBook parsedBook = [&]{
            auto _ = measureStage(CImportPerfTracker::EStage::Parse);
            return m_parserRegistry.Parse(request.SourcePath, sourceLabel);
        }();

        std::optional<std::string> effectiveSha256Hex = request.Sha256Hex;
        if (!effectiveSha256Hex.has_value())
        {
            try
            {
                auto _ = measureStage(CImportPerfTracker::EStage::Hash);
                effectiveSha256Hex = Librova::Hashing::ComputeFileSha256Hex(request.SourcePath);
            }
            catch (const std::exception& ex)
            {
                if (Librova::Logging::CLogging::IsInitialized())
                {
                    Librova::Logging::Warn(
                        "SHA-256 computation failed for '{}': {}",
                        sourceLabel,
                        ex.what());
                }
            }
        }

        const auto duplicates = [&]{
            auto _ = measureStage(CImportPerfTracker::EStage::FindDuplicates);
            return m_queryRepository.FindDuplicates(BuildCandidateBook(parsedBook, effectiveSha256Hex));
        }();

        if (IsCancellationRequested(stopToken, progressSink))
        {
            return buildCancelledResult(parsedBook.SourceFormat, duplicates, {});
        }

        if (HasStrictDuplicate(duplicates) && !request.AllowProbableDuplicates)
        {
            LogDuplicateDecision(request, duplicates, "rejected", true);
            LogDuplicateCandidateDetails(request, parsedBook.Metadata, effectiveSha256Hex, duplicates, true);
            return {
                .Status = ESingleFileImportStatus::RejectedDuplicate,
                .StoredFormat = parsedBook.SourceFormat,
                .DuplicateMatches = duplicates,
                .Warnings = {GStrictDuplicateWarning}
            };
        }

        if (HasProbableDuplicate(duplicates) && !request.AllowProbableDuplicates)
        {
            LogDuplicateDecision(request, duplicates, "rejected", true);
            LogDuplicateCandidateDetails(request, parsedBook.Metadata, effectiveSha256Hex, duplicates, true);
            return {
                .Status = ESingleFileImportStatus::RejectedDuplicate,
                .StoredFormat = parsedBook.SourceFormat,
                .DuplicateMatches = duplicates,
                .Warnings = {GProbableDuplicateWarning}
            };
        }

        const Librova::Domain::SBookId reservedBookId =
            request.PreReservedBookId.value_or(repo.ReserveId());
        progressSink.ReportValue(30, "Planning conversion");

        std::vector<std::string> importWarnings;

        if (HasStrictDuplicate(duplicates) && request.AllowProbableDuplicates)
        {
            importWarnings.emplace_back(GForcedStrictDuplicateWarning);
        }

        if (HasProbableDuplicate(duplicates) && request.AllowProbableDuplicates)
        {
            importWarnings.emplace_back(GForcedProbableDuplicateWarning);
        }

        if (!duplicates.empty() && request.AllowProbableDuplicates)
        {
            LogDuplicateDecision(request, duplicates, "allowed-override", false);
            LogDuplicateCandidateDetails(request, parsedBook.Metadata, effectiveSha256Hex, duplicates, false);
        }

        const std::filesystem::path convertedDestinationPath =
            request.WorkingDirectory / ("converted-" + std::to_string(reservedBookId.Value) + ".epub");
        const auto conversionPlan = Librova::ImportConversion::PlanImportConversion(
            request.SourcePath,
            parsedBook.SourceFormat,
            convertedDestinationPath,
            m_converter,
            request.ForceEpubConversion);

        std::optional<Librova::Domain::SConversionResult> conversionResult;

        if (conversionPlan.Request.has_value())
        {
            progressSink.ReportValue(45, "Running external conversion");
            auto _ = measureStage(CImportPerfTracker::EStage::Convert);

            if (Librova::Logging::CLogging::IsInitialized())
            {
                if (request.ImportJobId != 0)
                {
                    Librova::Logging::Info(
                        "convert: start job={} source='{}' format={}",
                        request.ImportJobId,
                        sourceLabel,
                        static_cast<int>(parsedBook.SourceFormat));
                }
                else
                {
                    Librova::Logging::Info(
                        "convert: start source='{}' format={}",
                        sourceLabel,
                        static_cast<int>(parsedBook.SourceFormat));
                }
            }

            conversionResult = m_converter->Convert(*conversionPlan.Request, progressSink, stopToken);

            if (Librova::Logging::CLogging::IsInitialized())
            {
                LogConversionOutcome(request, *conversionResult);
            }

            if (conversionResult->HasOutput())
            {
                temporaryConvertedPath = conversionResult->OutputPath;
            }
        }

        const auto conversionOutcome =
            Librova::ImportConversion::ResolveImportConversion(conversionPlan, conversionResult);

        if (conversionOutcome.Decision == Librova::ImportConversion::EImportConversionDecision::CancelImport)
        {
            importWarnings.insert(
                importWarnings.end(),
                conversionOutcome.Warnings.begin(),
                conversionOutcome.Warnings.end());

            return buildCancelledResult(parsedBook.SourceFormat, duplicates, std::move(importWarnings));
        }

        if (conversionOutcome.Decision == Librova::ImportConversion::EImportConversionDecision::FailImport)
        {
            importWarnings.insert(
                importWarnings.end(),
                conversionOutcome.Warnings.begin(),
                conversionOutcome.Warnings.end());

            return buildFailedResult(
                conversionOutcome.Error,
                std::move(importWarnings),
                conversionOutcome.Error);
        }

        if (parsedBook.HasCover())
        {
            auto _ = measureStage(CImportPerfTracker::EStage::Cover);
            try
            {
                const auto managedCover = ResolveManagedCoverData(request, parsedBook, m_coverImageProcessor);
                temporaryCoverPath = WriteCoverTempFile(
                    request.WorkingDirectory,
                    reservedBookId,
                    managedCover.Cover.Extension,
                    managedCover.Cover.Bytes);
                LogCoverOutcome(
                    request,
                    "stored",
                    managedCover.FinalReason,
                    managedCover.Cover.Extension,
                    managedCover.Cover.Bytes.size());
            }
            catch (const std::exception&)
            {
                LogCoverOutcome(request, "not-stored", "save-temp-failed", "<none>", 0, true);
                throw;
            }
        }
        else if (parsedBook.CoverDiagnosticMessage.has_value())
        {
            LogCoverOutcome(request, "not-stored", *parsedBook.CoverDiagnosticMessage, "<none>", 0, true);
        }
        else
        {
            LogCoverOutcome(request, "absent", "source-has-no-cover");
        }

        if (IsCancellationRequested(stopToken, progressSink))
        {
            return buildCancelledResult(conversionOutcome.Format, duplicates, std::move(importWarnings));
        }

        progressSink.ReportValue(70, "Preparing managed storage");
        const Librova::Domain::EStorageEncoding storageEncoding =
            DetermineStorageEncoding(conversionOutcome.Format, request.ForceEpubConversion);
        {
            auto _ = measureStage(CImportPerfTracker::EStage::PrepareStorage);
            preparedStorage = m_managedStorage.PrepareImport({
                .BookId = reservedBookId,
                .Format = conversionOutcome.Format,
                .StorageEncoding = storageEncoding,
                .SourcePath = conversionOutcome.SourcePath,
                .CoverSourcePath = temporaryCoverPath
            });
        }

        if (IsCancellationRequested(stopToken, progressSink))
        {
            return buildCancelledResult(conversionOutcome.Format, duplicates, std::move(importWarnings));
        }

        Librova::Domain::SBook book{
            .Id = reservedBookId,
            .Metadata = parsedBook.Metadata,
            .File = {
                .Format = conversionOutcome.Format,
                .StorageEncoding = storageEncoding,
                .ManagedPath = preparedStorage->RelativeBookPath,
                .SizeBytes = std::filesystem::file_size(preparedStorage->StagedBookPath),
                .Sha256Hex = effectiveSha256Hex.value_or("")
            },
            .CoverPath = preparedStorage->RelativeCoverPath,
            .AddedAtUtc = std::chrono::system_clock::now()
        };

        progressSink.ReportValue(85, "Writing book metadata");

        try
        {
            auto _ = measureStage(CImportPerfTracker::EStage::DbWriteWait);
            addedBookId = repo.Add(book);
        }
        catch (const Librova::Domain::CDuplicateHashException& duplicateEx)
        {
            if (request.AllowProbableDuplicates)
            {
                if (std::find(importWarnings.begin(), importWarnings.end(), GForcedStrictDuplicateWarning) == importWarnings.end())
                {
                    importWarnings.emplace_back(GForcedStrictDuplicateWarning);
                }

                if (Librova::Logging::CLogging::IsInitialized())
                {
                    Librova::Logging::Warn(
                        "duplicate: hash collision at write-time, overriding. title='{}' hash='{}' existingBookId={} source='{}'",
                        parsedBook.Metadata.TitleUtf8,
                        effectiveSha256Hex.value_or("<none>"),
                        duplicateEx.ExistingBookId().Value,
                        sourceLabel);
                }

                {
                    auto _ = measureStage(CImportPerfTracker::EStage::DbWriteWait);
                    addedBookId = repo.ForceAdd(book);
                }
            }
            else
            {
                m_managedStorage.RollbackImport(*preparedStorage);
                RemovePathNoThrow(temporaryConvertedPath.value_or(std::filesystem::path{}));
                RemovePathNoThrow(temporaryCoverPath.value_or(std::filesystem::path{}));
                importWarnings.push_back(GStrictDuplicateWarning);

                if (Librova::Logging::CLogging::IsInitialized())
                {
                    Librova::Logging::Warn(
                        "duplicate: rejected at write-time by hash collision. title='{}' hash='{}' existingBookId={} source='{}'",
                        parsedBook.Metadata.TitleUtf8,
                        effectiveSha256Hex.value_or("<none>"),
                        duplicateEx.ExistingBookId().Value,
                        sourceLabel);
                }

                LogDuplicateDecision(
                    request,
                    {Librova::Domain::SDuplicateMatch{
                        .Severity = Librova::Domain::EDuplicateSeverity::Strict,
                        .Reason = Librova::Domain::EDuplicateReason::SameHash,
                        .ExistingBookId = duplicateEx.ExistingBookId()
                    }},
                    "rejected-write-time-hash-collision",
                    true);

                return {
                    .Status = ESingleFileImportStatus::RejectedDuplicate,
                    .StoredFormat = conversionOutcome.Format,
                    .DuplicateMatches = {Librova::Domain::SDuplicateMatch{
                        .Severity = Librova::Domain::EDuplicateSeverity::Strict,
                        .Reason = Librova::Domain::EDuplicateReason::SameHash,
                        .ExistingBookId = duplicateEx.ExistingBookId()
                    }},
                    .Warnings = std::move(importWarnings)
                };
            }
        }

        progressSink.ReportValue(95, "Committing managed files");
        {
            auto _ = measureStage(CImportPerfTracker::EStage::CommitStorage);
            m_managedStorage.CommitImport(*preparedStorage);
        }

        RemovePathNoThrow(temporaryConvertedPath.value_or(std::filesystem::path{}));
        RemovePathNoThrow(temporaryCoverPath.value_or(std::filesystem::path{}));
        progressSink.ReportValue(100, "Import completed");

        return {
            .Status = ESingleFileImportStatus::Imported,
            .ImportedBookId = addedBookId,
            .StoredFormat = conversionOutcome.Format,
            .DuplicateMatches = duplicates,
            .Warnings = [&] {
                importWarnings.insert(
                    importWarnings.end(),
                    conversionOutcome.Warnings.begin(),
                    conversionOutcome.Warnings.end());
                return importWarnings;
            }()
        };
    }
    catch (const std::exception& error)
    {
        const std::string diagnosticError = error.what();
        const std::string transportError = SanitizeTransportErrorMessage(diagnosticError);

        if (Librova::Logging::CLogging::IsInitialized())
        {
            if (request.ImportJobId != 0)
            {
                Librova::Logging::Error(
                    "Single file import failed: job={} source='{}' reason='{}'",
                    request.ImportJobId,
                    sourceLabel,
                    diagnosticError);
            }
            else
            {
                Librova::Logging::Error(
                    "Single file import failed: source='{}' reason='{}'",
                    sourceLabel,
                    diagnosticError);
            }
        }

        if (addedBookId.has_value())
        {
            try
            {
                repo.Remove(*addedBookId);
            }
            catch (...)
            {
            }
        }

        if (preparedStorage.has_value())
        {
            m_managedStorage.RollbackImport(*preparedStorage);
        }

        RemovePathNoThrow(temporaryConvertedPath.value_or(std::filesystem::path{}));
        RemovePathNoThrow(temporaryCoverPath.value_or(std::filesystem::path{}));

        return {
            .Status = ESingleFileImportStatus::Failed,
            .Error = std::move(transportError),
            .DiagnosticError = std::move(diagnosticError)
        };
    }
}

} // namespace Librova::Importing
