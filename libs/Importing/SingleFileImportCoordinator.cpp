#include "Importing/SingleFileImportCoordinator.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

constexpr auto GStrictDuplicateWarning = "Import rejected because a strict duplicate already exists.";
constexpr auto GProbableDuplicateWarning = "Import requires user confirmation because a probable duplicate was found.";

void EnsureDirectory(const std::filesystem::path& path)
{
    std::error_code errorCode;
    std::filesystem::create_directories(path, errorCode);

    if (errorCode)
    {
        throw std::runtime_error(std::string{"Failed to create directory: "} + path.string());
    }
}

std::filesystem::path WriteCoverTempFile(
    const std::filesystem::path& workingDirectory,
    const LibriFlow::Domain::SBookId bookId,
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

bool HasStrictDuplicate(const std::vector<LibriFlow::Domain::SDuplicateMatch>& duplicates)
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

bool HasProbableDuplicate(const std::vector<LibriFlow::Domain::SDuplicateMatch>& duplicates)
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

} // namespace

namespace LibriFlow::Importing {

CSingleFileImportCoordinator::CSingleFileImportCoordinator(
    const LibriFlow::ParserRegistry::CBookParserRegistry& parserRegistry,
    LibriFlow::Domain::IBookRepository& bookRepository,
    const LibriFlow::Domain::IBookQueryRepository& queryRepository,
    LibriFlow::Domain::IManagedStorage& managedStorage,
    const LibriFlow::Domain::IBookConverter* converter)
    : m_parserRegistry(parserRegistry)
    , m_bookRepository(bookRepository)
    , m_queryRepository(queryRepository)
    , m_managedStorage(managedStorage)
    , m_converter(converter)
{
}

LibriFlow::Domain::SCandidateBook CSingleFileImportCoordinator::BuildCandidateBook(
    const LibriFlow::Domain::SParsedBook& parsedBook,
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
    LibriFlow::Domain::IProgressSink& progressSink,
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
    std::optional<LibriFlow::Domain::SPreparedStorage> preparedStorage;
    std::optional<LibriFlow::Domain::SBookId> addedBookId;

    try
    {
        progressSink.ReportValue(5, "Detecting source format");
        const std::optional<LibriFlow::Domain::EBookFormat> detectedFormat =
            LibriFlow::ParserRegistry::CBookParserRegistry::TryDetectFormat(request.SourcePath);

        if (!detectedFormat.has_value())
        {
            return {
                .Status = ESingleFileImportStatus::UnsupportedFormat,
                .Error = "Unsupported input format."
            };
        }

        progressSink.ReportValue(15, "Parsing source book");
        const LibriFlow::Domain::SParsedBook parsedBook = m_parserRegistry.Parse(request.SourcePath);
        const auto duplicates = m_queryRepository.FindDuplicates(BuildCandidateBook(parsedBook, request.Sha256Hex));

        if (HasStrictDuplicate(duplicates))
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

        const LibriFlow::Domain::SBookId reservedBookId = m_bookRepository.ReserveId();
        progressSink.ReportValue(30, "Planning conversion");

        const std::filesystem::path convertedDestinationPath =
            request.WorkingDirectory / ("converted-" + std::to_string(reservedBookId.Value) + ".epub");
        const auto conversionPlan = LibriFlow::ImportConversion::PlanImportConversion(
            request.SourcePath,
            parsedBook.SourceFormat,
            convertedDestinationPath,
            m_converter);

        std::optional<LibriFlow::Domain::SConversionResult> conversionResult;

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
            LibriFlow::ImportConversion::ResolveImportConversion(conversionPlan, conversionResult);

        if (conversionOutcome.Decision == LibriFlow::ImportConversion::EImportConversionDecision::CancelImport)
        {
            return {
                .Status = ESingleFileImportStatus::Cancelled,
                .StoredFormat = parsedBook.SourceFormat,
                .Warnings = conversionOutcome.Warnings
            };
        }

        if (parsedBook.HasCover())
        {
            temporaryCoverPath = WriteCoverTempFile(
                request.WorkingDirectory,
                reservedBookId,
                *parsedBook.CoverExtension,
                parsedBook.CoverBytes);
        }

        progressSink.ReportValue(70, "Preparing managed storage");
        preparedStorage = m_managedStorage.PrepareImport({
            .BookId = reservedBookId,
            .Format = conversionOutcome.Format,
            .SourcePath = conversionOutcome.SourcePath,
            .CoverSourcePath = temporaryCoverPath
        });

        LibriFlow::Domain::SBook book{
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
            .Warnings = conversionOutcome.Warnings
        };
    }
    catch (const std::exception& error)
    {
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
            .Error = error.what()
        };
    }
}

} // namespace LibriFlow::Importing
