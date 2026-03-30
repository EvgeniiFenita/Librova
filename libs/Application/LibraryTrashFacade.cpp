#include "Application/LibraryTrashFacade.hpp"

#include <stdexcept>

namespace Librova::Application {
namespace {

[[nodiscard]] bool IsSafeRelativeManagedPath(const std::filesystem::path& path)
{
    if (path.empty() || path.is_absolute())
    {
        return false;
    }

    const auto normalized = path.lexically_normal();
    if (normalized.empty() || normalized.is_absolute())
    {
        return false;
    }

    for (const auto& part : normalized)
    {
        if (part == "..")
        {
            return false;
        }
    }

    return true;
}

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
            catch (...)
            {
            }
        }

        if (trashedBookPath.has_value())
        {
            try
            {
                m_trashService.RestoreFromTrash(*trashedBookPath, sourceBookPath);
            }
            catch (...)
            {
            }
        }

        throw;
    }
}

std::filesystem::path CLibraryTrashFacade::ResolveManagedSourcePath(const std::filesystem::path& managedPath) const
{
    const auto normalizedManagedPath = managedPath.lexically_normal();
    if (normalizedManagedPath.is_absolute())
    {
        if (!IsPathWithinRoot(m_libraryRoot, normalizedManagedPath))
        {
            throw std::runtime_error("Managed source path is unsafe.");
        }

        return normalizedManagedPath;
    }

    if (!IsSafeRelativeManagedPath(normalizedManagedPath))
    {
        throw std::runtime_error("Managed source path is unsafe.");
    }

    return (m_libraryRoot / normalizedManagedPath).lexically_normal();
}

} // namespace Librova::Application
