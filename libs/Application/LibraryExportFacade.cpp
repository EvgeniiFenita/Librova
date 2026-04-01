#include "Application/LibraryExportFacade.hpp"

#include <stdexcept>

#include "Logging/Logging.hpp"
#include "ManagedPaths/ManagedPathSafety.hpp"

namespace Librova::Application {
namespace {

} // namespace

CLibraryExportFacade::CLibraryExportFacade(
    const Librova::Domain::IBookRepository& bookRepository,
    std::filesystem::path libraryRoot)
    : m_bookRepository(bookRepository)
    , m_libraryRoot(std::move(libraryRoot))
{
}

std::optional<std::filesystem::path> CLibraryExportFacade::ExportBook(
    const Librova::Domain::SBookId id,
    const std::filesystem::path& destinationPath) const
{
    if (destinationPath.empty())
    {
        throw std::invalid_argument("Export destination path cannot be empty.");
    }

    if (!destinationPath.is_absolute())
    {
        throw std::invalid_argument("Export destination path must be absolute.");
    }

    const auto book = m_bookRepository.GetById(id);
    if (!book.has_value())
    {
        return std::nullopt;
    }

    if (!book->File.HasManagedPath())
    {
        throw std::runtime_error("Book does not have a managed file path.");
    }

    const auto sourcePath = ResolveManagedSourcePath(book->File.ManagedPath);

    if (std::filesystem::is_directory(destinationPath))
    {
        throw std::invalid_argument("Export destination path must point to a file.");
    }

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
