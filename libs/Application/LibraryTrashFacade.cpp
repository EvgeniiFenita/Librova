#include "Application/LibraryTrashFacade.hpp"

#include <stdexcept>

#include "Logging/Logging.hpp"
#include "ManagedPaths/ManagedPathSafety.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace Librova::Application {
namespace {

void LogRollbackFailure(const std::string_view label, const std::filesystem::path& sourcePath, const std::exception& error) noexcept
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    try
    {
        Librova::Logging::Error(
            "Failed to restore {} during trash rollback. Path='{}' Error='{}'.",
            label,
            Librova::Unicode::PathToUtf8(sourcePath),
            error.what());
    }
    catch (...)
    {
    }
}

} // namespace

CLibraryTrashFacade::CLibraryTrashFacade(
    Librova::Domain::IBookRepository& bookRepository,
    Librova::Domain::ITrashService& trashService,
    std::filesystem::path libraryRoot)
    : m_bookRepository(bookRepository)
    , m_trashService(trashService)
    , m_libraryRoot(std::move(libraryRoot))
{
}

std::optional<STrashedBookResult> CLibraryTrashFacade::MoveBookToTrash(const Librova::Domain::SBookId id) const
{
    const auto book = m_bookRepository.GetById(id);
    if (!book.has_value())
    {
        return std::nullopt;
    }

    if (!book->File.HasManagedPath())
    {
        throw std::runtime_error("Book does not have a managed file path.");
    }

    const auto sourceBookPath = ResolveManagedSourcePath(book->File.ManagedPath);
    const auto sourceCoverPath = book->CoverPath.has_value()
        ? std::make_optional(ResolveManagedSourcePath(*book->CoverPath))
        : std::nullopt;

    std::optional<std::filesystem::path> trashedBookPath;
    std::optional<std::filesystem::path> trashedCoverPath;

    try
    {
        trashedBookPath = m_trashService.MoveToTrash(sourceBookPath);

        if (sourceCoverPath.has_value())
        {
            trashedCoverPath = m_trashService.MoveToTrash(*sourceCoverPath);
        }

        m_bookRepository.Remove(id);

        return STrashedBookResult{
            .BookId = id,
            .TrashedBookPath = *trashedBookPath,
            .TrashedCoverPath = trashedCoverPath
        };
    }
    catch (...)
    {
        if (trashedCoverPath.has_value() && sourceCoverPath.has_value())
        {
            try
            {
                m_trashService.RestoreFromTrash(*trashedCoverPath, *sourceCoverPath);
            }
            catch (const std::exception& error)
            {
                LogRollbackFailure("cover", *sourceCoverPath, error);
            }
        }

        if (trashedBookPath.has_value())
        {
            try
            {
                m_trashService.RestoreFromTrash(*trashedBookPath, sourceBookPath);
            }
            catch (const std::exception& error)
            {
                LogRollbackFailure("book", sourceBookPath, error);
            }
        }

        throw;
    }
}

std::filesystem::path CLibraryTrashFacade::ResolveManagedSourcePath(const std::filesystem::path& managedPath) const
{
    return Librova::ManagedPaths::ResolveExistingPathWithinRoot(
        m_libraryRoot,
        managedPath,
        "Managed source file does not exist.",
        "Managed source path is unsafe.",
        "Managed source path could not be canonicalized.");
}

} // namespace Librova::Application
