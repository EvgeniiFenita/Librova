#include "App/LibraryTrashFacade.hpp"

#include <stdexcept>

#include "Domain/DomainError.hpp"
#include "Foundation/Logging.hpp"
#include "Storage/ManagedPathSafety.hpp"
#include "Storage/ManagedLibraryLayout.hpp"
#include "Foundation/UnicodeConversion.hpp"

namespace Librova::Application {
namespace {

struct SStagedTrashEntry
{
    std::string Label;
    std::filesystem::path OriginalPath;
    std::filesystem::path TrashedPath;
};

void LogTrashStageFailure(const std::string_view label, const std::filesystem::path& sourcePath, const std::exception& error) noexcept
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    try
    {
        Librova::Logging::Warn(
            "Failed to stage {} into managed Trash before catalog removal. Path='{}' Error='{}'.",
            label,
            Librova::Unicode::PathToUtf8(sourcePath),
            error.what());
    }
    catch (const std::exception& loggingError)
    {
        try
        {
            Librova::Logging::Warn(
                "Failed to write trash-stage diagnostics. Label='{}' Error='{}' LoggingError='{}'.",
                label,
                error.what(),
                loggingError.what());
        }
        catch (...)
        {
        }
    }
    catch (...)
    {
    }
}

void LogTrashRestoreFailure(
    const std::string_view label,
    const std::filesystem::path& trashedPath,
    const std::filesystem::path& destinationPath,
    const std::exception& error) noexcept
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    try
    {
        Librova::Logging::Error(
            "Failed to restore staged {} from managed Trash after catalog removal failed. TrashedPath='{}' Destination='{}' Error='{}'.",
            label,
            Librova::Unicode::PathToUtf8(trashedPath),
            Librova::Unicode::PathToUtf8(destinationPath),
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
    catch (const std::exception& loggingError)
    {
        try
        {
            Librova::Logging::Warn(
                "Failed to write recycle-bin fallback diagnostics for deleted book {}. Error='{}' LoggingError='{}'.",
                bookId.Value,
                error.what(),
                loggingError.what());
        }
        catch (...)
        {
        }
    }
    catch (...)
    {
    }
}

void RestoreStagedPathsOrThrow(
    Librova::Domain::ITrashService& trashService,
    const std::vector<SStagedTrashEntry>& stagedEntries)
{
    for (auto it = stagedEntries.rbegin(); it != stagedEntries.rend(); ++it)
    {
        try
        {
            trashService.RestoreFromTrash(it->TrashedPath, it->OriginalPath);
        }
        catch (const std::exception& error)
        {
            LogTrashRestoreFailure(it->Label, it->TrashedPath, it->OriginalPath, error);
            throw;
        }
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
        throw Librova::Domain::CDomainException(
            Librova::Domain::EDomainErrorCode::IntegrityIssue,
            "Book does not have a managed file path.");
    }

    const auto sourceBookPath = ResolveManagedSourcePathIfPresent(book->File.ManagedPath);
    const auto sourceCoverPath = book->CoverPath.has_value()
        ? ResolveManagedSourcePathIfPresent(*book->CoverPath)
        : std::nullopt;
    const bool hasMissingManagedArtifacts = !sourceBookPath.has_value()
        || (book->CoverPath.has_value() && !sourceCoverPath.has_value());

    std::vector<SStagedTrashEntry> stagedEntries;
    std::vector<std::filesystem::path> stagedPaths;

    if (sourceBookPath.has_value())
    {
        try
        {
            const auto trashedPath = m_trashService.MoveToTrash(*sourceBookPath);
            stagedEntries.push_back({
                .Label = "book",
                .OriginalPath = *sourceBookPath,
                .TrashedPath = trashedPath
            });
            stagedPaths.push_back(std::move(trashedPath));
        }
        catch (const std::exception& error)
        {
            LogTrashStageFailure("book", *sourceBookPath, error);
            throw;
        }
    }

    if (sourceCoverPath.has_value())
    {
        try
        {
            const auto trashedPath = m_trashService.MoveToTrash(*sourceCoverPath);
            stagedEntries.push_back({
                .Label = "cover",
                .OriginalPath = *sourceCoverPath,
                .TrashedPath = trashedPath
            });
            stagedPaths.push_back(std::move(trashedPath));
        }
        catch (const std::exception& error)
        {
            LogTrashStageFailure("cover", *sourceCoverPath, error);
            RestoreStagedPathsOrThrow(m_trashService, stagedEntries);
            throw;
        }
    }

    try
    {
        m_bookRepository.Remove(id);
    }
    catch (...)
    {
        RestoreStagedPathsOrThrow(m_trashService, stagedEntries);
        throw;
    }

    const size_t expectedMoveCount = 1 + (sourceCoverPath.has_value() ? 1 : 0);
    const bool hasOrphanedFiles = hasMissingManagedArtifacts || stagedEntries.size() < expectedMoveCount;
    if (m_recycleBinService == nullptr || stagedPaths.empty())
    {
        return STrashedBookResult{
            .BookId = id,
            .Destination = ETrashDestination::ManagedTrash,
            .HasOrphanedFiles = hasOrphanedFiles
        };
    }

    try
    {
        m_recycleBinService->MoveToRecycleBin(stagedPaths);
        const auto trashRoot = Librova::StoragePlanning::CManagedLibraryLayout::Build(m_libraryRoot).TrashDirectory;
        const auto trashObjectsRoot = trashRoot / "Objects";
        for (const auto& stagedPath : stagedPaths)
        {
            (void)Librova::ManagedPaths::CleanupEmptyDirectoriesUpTo(stagedPath.parent_path(), trashObjectsRoot);
        }

        return STrashedBookResult{
            .BookId = id,
            .Destination = ETrashDestination::RecycleBin,
            .HasOrphanedFiles = hasOrphanedFiles
        };
    }
    catch (const std::exception& error)
    {
        LogRecycleBinFallback(id, stagedPaths, error);
        return STrashedBookResult{
            .BookId = id,
            .Destination = ETrashDestination::ManagedTrash,
            .HasOrphanedFiles = hasOrphanedFiles
        };
    }
}

std::optional<std::filesystem::path> CLibraryTrashFacade::ResolveManagedSourcePathIfPresent(
    const std::filesystem::path& managedPath) const
{
    return Librova::ManagedPaths::TryResolvePathWithinRoot(
        m_libraryRoot,
        managedPath,
        "Managed source path is unsafe.",
        "Managed source path could not be canonicalized.");
}

} // namespace Librova::Application
