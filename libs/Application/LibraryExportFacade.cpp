#include "Application/LibraryExportFacade.hpp"

#include <stdexcept>

#include "Logging/Logging.hpp"
#include "ManagedPaths/ManagedPathSafety.hpp"

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

    if (!request.ExportFormat.has_value() || *request.ExportFormat == book->File.Format)
    {
        return ExportManagedFile(sourcePath, request.DestinationPath);
    }

    return ExportConvertedFile(
        *book,
        sourcePath,
        request.DestinationPath,
        *request.ExportFormat);
}

std::filesystem::path CLibraryExportFacade::ExportManagedFile(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& destinationPath) const
{
    if (!destinationPath.parent_path().empty())
    {
        std::filesystem::create_directories(destinationPath.parent_path());
    }

    if (std::filesystem::exists(destinationPath) && Librova::Logging::CLogging::IsInitialized())
    {
        Librova::Logging::Warn(
            "Export destination already exists and will be overwritten. Destination='{}'.",
            Librova::ManagedPaths::PathToUtf8(destinationPath));
    }

    std::filesystem::copy_file(
        sourcePath,
        destinationPath,
        std::filesystem::copy_options::overwrite_existing);

    if (Librova::Logging::CLogging::IsInitialized())
    {
        Librova::Logging::Info(
            "Exported managed book to '{}'.",
            Librova::ManagedPaths::PathToUtf8(destinationPath));
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
            Librova::ManagedPaths::PathToUtf8(destinationPath));
    }

    CNoOpProgressSink progressSink;
    const auto conversionResult = m_converter->Convert(
        Librova::Domain::SConversionRequest{
            .SourcePath = sourcePath,
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
            Librova::ManagedPaths::PathToUtf8(exportedPath));
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
