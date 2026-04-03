#include "Importing/SingleFileImportCoordinator.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "Logging/Logging.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace {

constexpr auto GStrictDuplicateWarning = "Import rejected because a strict duplicate already exists.";
constexpr auto GProbableDuplicateWarning = "Import requires user confirmation because a probable duplicate was found.";
constexpr auto GForcedStrictDuplicateWarning = "Import continued with explicit strict duplicate override.";
constexpr auto GForcedProbableDuplicateWarning = "Import continued with explicit probable duplicate override.";
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
        if (duplicate.RequiresUserDecision())
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
                Librova::Logging::Info(
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
        Librova::Logging::Warn(
            "Managed cover optimization skipped for '{}': status='{}' reason='{}'",
            Librova::Unicode::PathToUtf8(sourcePath),
            processingResult.Status == Librova::Domain::ECoverProcessingStatus::Unsupported ? "unsupported" : "failed",
            processingResult.DiagnosticMessage);
    }

    return sourceCover;
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
        const auto duplicates = m_queryRepository.FindDuplicates(BuildCandidateBook(parsedBook, request.Sha256Hex));

        if (IsCancellationRequested(stopToken, progressSink))
        {
            return buildCancelledResult(parsedBook.SourceFormat, duplicates, {});
        }

        if (HasStrictDuplicate(duplicates) && !request.AllowProbableDuplicates)
        {
            return {
                .Status = ESingleFileImportStatus::RejectedDuplicate,
                .StoredFormat = parsedBook.SourceFormat,
                .DuplicateMatches = duplicates,
                .Warnings = {GStrictDuplicateWarning}
            };
        }

        if (HasProbableDuplicate(duplicates) && !request.AllowProbableDuplicates)
        {
            return {
                .Status = ESingleFileImportStatus::DecisionRequired,
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
        preparedStorage = m_managedStorage.PrepareImport({
            .BookId = reservedBookId,
            .Format = conversionOutcome.Format,
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
                .ManagedPath = preparedStorage->FinalBookPath,
                .SizeBytes = std::filesystem::file_size(conversionOutcome.SourcePath),
                .Sha256Hex = request.Sha256Hex.value_or("")
            },
            .CoverPath = preparedStorage->FinalCoverPath,
            .AddedAtUtc = std::chrono::system_clock::now()
        };

        progressSink.ReportValue(85, "Writing book metadata");
        addedBookId = m_bookRepository.Add(book);

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
