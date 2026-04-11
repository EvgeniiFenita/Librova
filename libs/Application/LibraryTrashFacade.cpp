#include "Application/LibraryTrashFacade.hpp"

#include <stdexcept>

#include "Domain/DomainError.hpp"
#include "Logging/Logging.hpp"
#include "ManagedPaths/ManagedPathSafety.hpp"
#include "StoragePlanning/ManagedLibraryLayout.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace Librova::Application {
namespace {

void LogOrphanedFile(const std::string_view label, const std::filesystem::path& sourcePath, const std::exception& error) noexcept
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    try
    {
        Librova::Logging::Warn(
            "Failed to move {} to trash after catalog removal; file left as orphan. Path='{}' Error='{}'.",
            label,
            Librova::Unicode::PathToUtf8(sourcePath),
            error.what());
    }
    catch (const std::exception& loggingError)
    {
        try
        {
            Librova::Logging::Warn(
                "Failed to write orphaned-file diagnostics. Label='{}' Error='{}' LoggingError='{}'.",
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

    // Catalog removal is the commit point. Once this returns, the book is logically
    // deleted regardless of what happens to the files. If this throws, no files have
    // been touched — safe to propagate.
    m_bookRepository.Remove(id);

    // File moves are best-effort after the catalog commit. On failure the file is
    // left as an orphan in the managed library; the catalog remains consistent.
    std::vector<std::filesystem::path> stagedPaths;

    if (sourceBookPath.has_value())
    {
        try
        {
            stagedPaths.push_back(m_trashService.MoveToTrash(*sourceBookPath));
        }
        catch (const std::exception& error)
        {
            LogOrphanedFile("book", *sourceBookPath, error);
        }
    }

    if (sourceCoverPath.has_value())
    {
        try
        {
            stagedPaths.push_back(m_trashService.MoveToTrash(*sourceCoverPath));
        }
        catch (const std::exception& error)
        {
            LogOrphanedFile("cover", *sourceCoverPath, error);
        }
    }

    const size_t expectedMoveCount = 1 + (sourceCoverPath.has_value() ? 1 : 0);
    const bool hasOrphanedFiles = hasMissingManagedArtifacts || stagedPaths.size() < expectedMoveCount;
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
        for (const auto& stagedPath : stagedPaths)
        {
            CleanupEmptyDirectoriesUpToRoot(stagedPath, trashRoot);
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
