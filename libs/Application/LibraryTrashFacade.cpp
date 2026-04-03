#include "Application/LibraryTrashFacade.hpp"

#include <stdexcept>

#include "Logging/Logging.hpp"
#include "ManagedPaths/ManagedPathSafety.hpp"
#include "StoragePlanning/ManagedLibraryLayout.hpp"
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

void LogRecycleBinFallback(
    const Librova::Domain::SBookId bookId,
    const std::vector<std::filesystem::path>& stagedPaths,
    const std::exception& error) noexcept
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    try
    {
        std::string stagedPathList;
        for (std::size_t index = 0; index < stagedPaths.size(); ++index)
        {
            if (index > 0)
            {
                stagedPathList += "; ";
            }

            stagedPathList += Librova::Unicode::PathToUtf8(stagedPaths[index]);
        }

        Librova::Logging::Warn(
            "Failed to move deleted book {} from managed Trash to Windows Recycle Bin. "
            "Leaving staged files in library Trash. StagedPaths='{}' Error='{}'.",
            bookId.Value,
            stagedPathList,
            error.what());
    }
    catch (...)
    {
    }
}

void CleanupEmptyDirectoriesUpToRoot(const std::filesystem::path& path, const std::filesystem::path& rootPath) noexcept
{
    if (path.empty() || rootPath.empty())
    {
        return;
    }

    std::error_code errorCode;
    const auto normalizedRoot = std::filesystem::weakly_canonical(rootPath, errorCode);
    if (errorCode)
    {
        return;
    }

    auto currentPath = path.parent_path();
    while (!currentPath.empty())
    {
        const auto normalizedCurrent = std::filesystem::weakly_canonical(currentPath, errorCode);
        if (errorCode)
        {
            return;
        }

        if (normalizedCurrent == normalizedRoot)
        {
            return;
        }

        std::filesystem::remove(currentPath, errorCode);
        if (errorCode)
        {
            return;
        }

        if (std::filesystem::exists(currentPath))
        {
            return;
        }

        currentPath = currentPath.parent_path();
    }
}

} // namespace

CLibraryTrashFacade::CLibraryTrashFacade(
    Librova::Domain::IBookRepository& bookRepository,
    Librova::Domain::ITrashService& trashService,
    std::filesystem::path libraryRoot,
    Librova::Domain::IRecycleBinService* recycleBinService)
    : m_bookRepository(bookRepository)
    , m_trashService(trashService)
    , m_libraryRoot(std::move(libraryRoot))
    , m_recycleBinService(recycleBinService)
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

    std::optional<std::filesystem::path> stagedBookPath;
    std::optional<std::filesystem::path> stagedCoverPath;

    try
    {
        stagedBookPath = m_trashService.MoveToTrash(sourceBookPath);

        if (sourceCoverPath.has_value())
        {
            stagedCoverPath = m_trashService.MoveToTrash(*sourceCoverPath);
        }

        m_bookRepository.Remove(id);
    }
    catch (...)
    {
        if (stagedCoverPath.has_value() && sourceCoverPath.has_value())
        {
            try
            {
                m_trashService.RestoreFromTrash(*stagedCoverPath, *sourceCoverPath);
            }
            catch (const std::exception& error)
            {
                LogRollbackFailure("cover", *sourceCoverPath, error);
            }
        }

        if (stagedBookPath.has_value())
        {
            try
            {
                m_trashService.RestoreFromTrash(*stagedBookPath, sourceBookPath);
            }
            catch (const std::exception& error)
            {
                LogRollbackFailure("book", sourceBookPath, error);
            }
        }

        throw;
    }

    if (m_recycleBinService == nullptr)
    {
        return STrashedBookResult{
            .BookId = id,
            .Destination = ETrashDestination::ManagedTrash
        };
    }

    std::vector<std::filesystem::path> stagedPaths;
    stagedPaths.push_back(*stagedBookPath);
    if (stagedCoverPath.has_value())
    {
        stagedPaths.push_back(*stagedCoverPath);
    }

    try
    {
        m_recycleBinService->MoveToRecycleBin(stagedPaths);
        const auto trashRoot = Librova::StoragePlanning::CManagedLibraryLayout::Build(m_libraryRoot).TrashDirectory;
        for (const auto& stagedPath : stagedPaths)
        {
            CleanupEmptyDirectoriesUpToRoot(stagedPath, trashRoot);
        }

        return STrashedBookResult{
            .BookId = id,
            .Destination = ETrashDestination::RecycleBin
        };
    }
    catch (const std::exception& error)
    {
        LogRecycleBinFallback(id, stagedPaths, error);
        return STrashedBookResult{
            .BookId = id,
            .Destination = ETrashDestination::ManagedTrash
        };
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
