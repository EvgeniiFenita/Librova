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

void LogDuplicateCandidateDetails(
    const Librova::Domain::SBookMetadata& metadata,
    const std::optional<std::string>& sha256Hex,
    const std::vector<Librova::Domain::SDuplicateMatch>& matches)
{
    if (!Librova::Logging::CLogging::IsInitialized())
        return;

    const std::string authorsStr = JoinAuthors(metadata.AuthorsUtf8);

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

        Librova::Logging::Warn(
            "Duplicate candidate fields: title='{}' authors='{}' reason='{}' matchValue='{}' existingBookId={} existingTitle='{}' existingAuthors='{}'",
            metadata.TitleUtf8,
            authorsStr,
            DuplicateReasonLabel(match.Reason),
            matchDetail,
            match.ExistingBookId.Value,
            match.ExistingTitle,
            match.ExistingAuthors);
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

[[nodiscard]] Librova::Domain::SCoverData ResolveManagedCoverData(
    const Librova::Domain::SParsedBook& parsedBook,
    const Librova::Domain::ICoverImageProcessor* coverImageProcessor,
    const std::filesystem::path& sourcePath)
{
    const auto sourceCover = BuildSourceCoverData(parsedBook);

    if (coverImageProcessor == nullptr)
    {
        return sourceCover;
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
                Librova::Logging::Debug(
                    "Prepared managed cover for '{}': status='{}' output_extension='{}' dimensions={}x{} bytes={}",
                    Librova::Unicode::PathToUtf8(sourcePath),
                    processingResult.Status == Librova::Domain::ECoverProcessingStatus::Processed ? "processed" : "unchanged",
                    processingResult.Cover.Extension,
                    processingResult.PixelWidth,
                    processingResult.PixelHeight,
                    processingResult.Cover.Bytes.size());
            }

            return processingResult.Cover;
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
            "Managed cover optimization skipped for '{}': status='{}' reason='{}' input_extension='{}' input_bytes={}",
            Librova::Unicode::PathToUtf8(sourcePath),
            statusLabel,
            reason,
            sourceCover.Extension,
            sourceCover.Bytes.size());
    }

    return sourceCover;
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
        const Librova::Domain::SParsedBook parsedBook = m_parserRegistry.Parse(request.SourcePath);

        std::optional<std::string> effectiveSha256Hex = request.Sha256Hex;
        if (!effectiveSha256Hex.has_value())
        {
            try
            {
                effectiveSha256Hex = Librova::Hashing::ComputeFileSha256Hex(request.SourcePath);
            }
            catch (const std::exception& ex)
            {
                if (Librova::Logging::CLogging::IsInitialized())
                {
                    Librova::Logging::Warn(
                        "SHA-256 computation failed for '{}': {}",
                        Librova::Unicode::PathToUtf8(request.SourcePath),
                        ex.what());
                }
            }
        }

        const auto duplicates = m_queryRepository.FindDuplicates(BuildCandidateBook(parsedBook, effectiveSha256Hex));

        if (IsCancellationRequested(stopToken, progressSink))
        {
            return buildCancelledResult(parsedBook.SourceFormat, duplicates, {});
        }

        if (HasStrictDuplicate(duplicates) && !request.AllowProbableDuplicates)
        {
            LogDuplicateCandidateDetails(parsedBook.Metadata, effectiveSha256Hex, duplicates);
            return {
                .Status = ESingleFileImportStatus::RejectedDuplicate,
                .StoredFormat = parsedBook.SourceFormat,
                .DuplicateMatches = duplicates,
                .Warnings = {GStrictDuplicateWarning}
            };
        }

        if (HasProbableDuplicate(duplicates) && !request.AllowProbableDuplicates)
        {
            LogDuplicateCandidateDetails(parsedBook.Metadata, effectiveSha256Hex, duplicates);
            return {
                .Status = ESingleFileImportStatus::RejectedDuplicate,
                .StoredFormat = parsedBook.SourceFormat,
                .DuplicateMatches = duplicates,
                .Warnings = {GProbableDuplicateWarning}
            };
        }

        const Librova::Domain::SBookId reservedBookId = m_bookRepository.ReserveId();
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
            conversionResult = m_converter->Convert(*conversionPlan.Request, progressSink, stopToken);

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
            const auto managedCover = ResolveManagedCoverData(parsedBook, m_coverImageProcessor, request.SourcePath);
            temporaryCoverPath = WriteCoverTempFile(
                request.WorkingDirectory,
                reservedBookId,
                managedCover.Extension,
                managedCover.Bytes);
        }

        if (IsCancellationRequested(stopToken, progressSink))
        {
            return buildCancelledResult(conversionOutcome.Format, duplicates, std::move(importWarnings));
        }

        progressSink.ReportValue(70, "Preparing managed storage");
        const Librova::Domain::EStorageEncoding storageEncoding =
            DetermineStorageEncoding(conversionOutcome.Format, request.ForceEpubConversion);
        preparedStorage = m_managedStorage.PrepareImport({
            .BookId = reservedBookId,
            .Format = conversionOutcome.Format,
            .StorageEncoding = storageEncoding,
            .SourcePath = conversionOutcome.SourcePath,
            .CoverSourcePath = temporaryCoverPath
        });

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
            addedBookId = m_bookRepository.Add(book);
        }
        catch (const Librova::Domain::CDuplicateHashException& duplicateEx)
        {
            if (request.AllowProbableDuplicates)
            {
                if (std::find(importWarnings.begin(), importWarnings.end(), GForcedStrictDuplicateWarning) == importWarnings.end())
                {
                    importWarnings.emplace_back(GForcedStrictDuplicateWarning);
                }
                addedBookId = m_bookRepository.ForceAdd(book);
            }
            else
            {
                m_managedStorage.RollbackImport(*preparedStorage);
                RemovePathNoThrow(temporaryConvertedPath.value_or(std::filesystem::path{}));
                RemovePathNoThrow(temporaryCoverPath.value_or(std::filesystem::path{}));
                importWarnings.push_back(GStrictDuplicateWarning);

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
        m_managedStorage.CommitImport(*preparedStorage);

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
            Librova::Logging::Error(
                "Single file import failed: source='{}' reason='{}'",
                Librova::Unicode::PathToUtf8(request.SourcePath),
                diagnosticError);
        }

        if (addedBookId.has_value())
        {
            try
            {
                m_bookRepository.Remove(*addedBookId);
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
