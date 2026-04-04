#include "Application/LibraryExportFacade.hpp"

#include <chrono>
#include <stdexcept>

#include "Logging/Logging.hpp"
#include "ManagedFileEncoding/ManagedFileEncoding.hpp"
#include "ManagedPaths/ManagedPathSafety.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace Librova::Application {
namespace {

class CNoOpProgressSink final : public Librova::Domain::IProgressSink
{
public:
    void ReportValue(int, std::string_view) override
    {
    }

    [[nodiscard]] bool IsCancellationRequested() const override
    {
        return false;
    }
};

class CScopedPathCleanup final
{
public:
    explicit CScopedPathCleanup(std::filesystem::path path)
        : m_path(std::move(path))
    {
    }

    ~CScopedPathCleanup()
    {
        if (m_path.empty())
        {
            return;
        }

        std::error_code errorCode;
        std::filesystem::remove(m_path, errorCode);
    }

    [[nodiscard]] const std::filesystem::path& GetPath() const noexcept
    {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

[[nodiscard]] bool IsPathWithinRoot(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate)
{
    const auto normalizedRoot = root.lexically_normal();
    const auto normalizedCandidate = candidate.lexically_normal();

    auto rootIt = normalizedRoot.begin();
    auto candidateIt = normalizedCandidate.begin();

    for (; rootIt != normalizedRoot.end(); ++rootIt, ++candidateIt)
    {
        if (candidateIt == normalizedCandidate.end() || *rootIt != *candidateIt)
        {
            return false;
        }
    }

    return true;
}

std::filesystem::path CanonicalizeForComparison(const std::filesystem::path& path)
{
    const auto comparisonBase = path.has_filename() ? path.parent_path() : path;
    std::error_code errorCode;
    const auto canonicalBase = std::filesystem::weakly_canonical(comparisonBase, errorCode);
    if (errorCode)
    {
        throw std::runtime_error("Export destination path could not be canonicalized.");
    }

    if (path.has_filename())
    {
        return (canonicalBase / path.filename()).lexically_normal();
    }

    return canonicalBase.lexically_normal();
}

} // namespace

CLibraryExportFacade::CLibraryExportFacade(
    const Librova::Domain::IBookRepository& bookRepository,
    std::filesystem::path libraryRoot,
    const Librova::Domain::IBookConverter* converter)
    : m_bookRepository(bookRepository)
    , m_libraryRoot(std::move(libraryRoot))
    , m_converter(converter)
{
}

std::optional<std::filesystem::path> CLibraryExportFacade::ExportBook(
    const SExportBookRequest& request) const
{
    if (request.DestinationPath.empty())
    {
        throw std::invalid_argument("Export destination path cannot be empty.");
    }

    if (!request.DestinationPath.is_absolute())
    {
        throw std::invalid_argument("Export destination path must be absolute.");
    }

    const auto book = m_bookRepository.GetById(request.BookId);
    if (!book.has_value())
    {
        return std::nullopt;
    }

    if (!book->File.HasManagedPath())
    {
        throw std::runtime_error("Book does not have a managed file path.");
    }

    const auto sourcePath = ResolveManagedSourcePath(book->File.ManagedPath);

    if (std::filesystem::is_directory(request.DestinationPath))
    {
        throw std::invalid_argument("Export destination path must point to a file.");
    }

    const auto canonicalLibraryRoot = CanonicalizeForComparison(m_libraryRoot);
    const auto canonicalDestinationPath = CanonicalizeForComparison(request.DestinationPath);
    if (IsPathWithinRoot(canonicalLibraryRoot, canonicalDestinationPath))
    {
        throw std::invalid_argument("Export destination path must be outside the managed library.");
    }

    if (!request.ExportFormat.has_value() || *request.ExportFormat == book->File.Format)
    {
        return ExportManagedFile(sourcePath, request.DestinationPath, book->File.StorageEncoding);
    }

    return ExportConvertedFile(
        *book,
        sourcePath,
        request.DestinationPath,
        *request.ExportFormat);
}

std::filesystem::path CLibraryExportFacade::ExportManagedFile(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& destinationPath,
    const Librova::Domain::EStorageEncoding storageEncoding) const
{
    if (!destinationPath.parent_path().empty())
    {
        std::filesystem::create_directories(destinationPath.parent_path());
    }

    if (std::filesystem::exists(destinationPath) && Librova::Logging::CLogging::IsInitialized())
    {
        Librova::Logging::Warn(
            "Export destination already exists and will be overwritten. Destination='{}'.",
            Librova::Unicode::PathToUtf8(destinationPath));
    }

    Librova::ManagedFileEncoding::DecodeFileToPath(sourcePath, storageEncoding, destinationPath);

    if (Librova::Logging::CLogging::IsInitialized())
    {
        Librova::Logging::Info(
            "Exported managed book to '{}'.",
            Librova::Unicode::PathToUtf8(destinationPath));
    }

    return destinationPath;
}

std::filesystem::path CLibraryExportFacade::ExportConvertedFile(
    const Librova::Domain::SBook& book,
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& destinationPath,
    const Librova::Domain::EBookFormat exportFormat) const
{
    if (book.File.Format != Librova::Domain::EBookFormat::Fb2 || exportFormat != Librova::Domain::EBookFormat::Epub)
    {
        throw std::invalid_argument("Requested export conversion is not supported.");
    }

    if (m_converter == nullptr || !m_converter->CanConvert(book.File.Format, exportFormat))
    {
        throw std::runtime_error("Configured FB2 to EPUB export converter is unavailable.");
    }

    if (!destinationPath.parent_path().empty())
    {
        std::filesystem::create_directories(destinationPath.parent_path());
    }

    if (std::filesystem::exists(destinationPath) && Librova::Logging::CLogging::IsInitialized())
    {
        Librova::Logging::Warn(
            "Converted export destination already exists and will be overwritten. Destination='{}'.",
            Librova::Unicode::PathToUtf8(destinationPath));
    }

    std::optional<CScopedPathCleanup> temporaryDecodedSource;
    auto conversionSourcePath = sourcePath;

    if (book.File.StorageEncoding == Librova::Domain::EStorageEncoding::Compressed)
    {
        const auto tempDirectory = m_libraryRoot / "Temp" / "Export";
        std::filesystem::create_directories(tempDirectory);
        const auto uniqueSuffix = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const auto temporaryDecodedPath =
            tempDirectory / ("decoded-" + std::to_string(book.Id.Value) + "-" + uniqueSuffix + ".fb2");
        temporaryDecodedSource.emplace(temporaryDecodedPath);
        Librova::ManagedFileEncoding::DecodeFileToPath(sourcePath, book.File.StorageEncoding, temporaryDecodedPath);
        conversionSourcePath = temporaryDecodedSource->GetPath();
    }

    CNoOpProgressSink progressSink;
    const auto conversionResult = m_converter->Convert(
        Librova::Domain::SConversionRequest{
            .SourcePath = conversionSourcePath,
            .DestinationPath = destinationPath,
            .SourceFormat = book.File.Format,
            .DestinationFormat = exportFormat
        },
        progressSink,
        std::stop_token{});

    if (conversionResult.IsCancelled())
    {
        throw std::runtime_error("Export conversion was cancelled.");
    }

    if (!conversionResult.IsSuccess())
    {
        if (!conversionResult.Warnings.empty())
        {
            throw std::runtime_error(conversionResult.Warnings.front());
        }

        throw std::runtime_error("Export conversion failed.");
    }

    const auto exportedPath = conversionResult.HasOutput() ? conversionResult.OutputPath : destinationPath;
    if (Librova::Logging::CLogging::IsInitialized())
    {
        Librova::Logging::Info(
            "Exported converted book to '{}'.",
            Librova::Unicode::PathToUtf8(exportedPath));
    }

    return exportedPath;
}

std::filesystem::path CLibraryExportFacade::ResolveManagedSourcePath(const std::filesystem::path& managedPath) const
{
    return Librova::ManagedPaths::ResolveExistingPathWithinRoot(
        m_libraryRoot,
        managedPath,
        "Managed source file does not exist.",
        "Managed source path is unsafe.",
        "Managed source path could not be canonicalized.");
}

} // namespace Librova::Application
