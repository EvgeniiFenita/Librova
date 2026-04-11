#include "Application/ImportRollbackService.hpp"

#include <optional>
#include <system_error>

#include "Logging/Logging.hpp"
#include "ManagedPaths/ManagedPathSafety.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace {

[[nodiscard]] std::optional<std::filesystem::path> TryResolveManagedPathWithinRoot(
    const std::filesystem::path& root,
    const std::filesystem::path& managedPath)
{
    if (managedPath.empty())
    {
        return std::nullopt;
    }

    const auto normalizedManagedPath = managedPath.lexically_normal();

    if (!normalizedManagedPath.is_absolute()
        && !Librova::ManagedPaths::IsSafeRelativeManagedPath(normalizedManagedPath))
    {
        return std::nullopt;
    }

    const std::filesystem::path candidatePath = normalizedManagedPath.is_absolute()
        ? normalizedManagedPath
        : (root / normalizedManagedPath).lexically_normal();

    if (!std::filesystem::exists(candidatePath))
    {
        return std::nullopt;
    }

    return Librova::ManagedPaths::ResolveExistingPathWithinRoot(
        root,
        managedPath,
        "Managed rollback path does not exist.",
        "Managed rollback path is unsafe.",
        "Managed rollback path could not be canonicalized.");
}

void CleanupEmptyParentsNoThrow(const std::filesystem::path& path, const std::filesystem::path& stopRoot) noexcept
{
    std::error_code errorCode;
    auto current = path.parent_path();
    const auto normalizedStopRoot = stopRoot.lexically_normal();

    while (!current.empty() && current.lexically_normal() != normalizedStopRoot)
    {
        if (!std::filesystem::remove(current, errorCode) || errorCode)
        {
            break;
        }

        current = current.parent_path();
    }
}

void LogRollbackCleanupIssueIfInitialized(
    const std::filesystem::path& managedPath,
    std::string_view reason) noexcept
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    try
    {
        Librova::Logging::Warn(
            "Import cancellation rollback could not clean up managed path '{}'. Reason='{}'.",
            Librova::Unicode::PathToUtf8(managedPath),
            reason);
    }
    catch (...)
    {
    }
}

[[nodiscard]] std::optional<std::string> CleanupManagedPathNoThrow(
    const std::filesystem::path& root,
    const std::filesystem::path& managedPath) noexcept
{
    if (managedPath.empty())
    {
        return std::nullopt;
    }

    const auto normalizedManagedPath = managedPath.lexically_normal();
    const auto candidatePath = normalizedManagedPath.is_absolute()
        ? normalizedManagedPath
        : (root / normalizedManagedPath).lexically_normal();

    try
    {
        if (!std::filesystem::exists(candidatePath))
        {
            return std::nullopt;
        }

        const auto resolvedPath = TryResolveManagedPathWithinRoot(root, normalizedManagedPath);
        if (!resolvedPath.has_value())
        {
            const std::string reason = "Path could not be resolved safely within the library root.";
            LogRollbackCleanupIssueIfInitialized(normalizedManagedPath, reason);
            return "Cancellation rollback left managed artifact '"
                + Librova::Unicode::PathToUtf8(candidatePath)
                + "' on disk because "
                + reason;
        }

        std::error_code errorCode;
        std::filesystem::remove(*resolvedPath, errorCode);
        if (errorCode)
        {
            const std::string reason = "Filesystem remove failed: " + errorCode.message();
            LogRollbackCleanupIssueIfInitialized(*resolvedPath, reason);
            return "Cancellation rollback left managed artifact '"
                + Librova::Unicode::PathToUtf8(*resolvedPath)
                + "' on disk because "
                + reason;
        }

        if (std::filesystem::exists(*resolvedPath))
        {
            const std::string reason = "Filesystem remove reported success but the path still exists.";
            LogRollbackCleanupIssueIfInitialized(*resolvedPath, reason);
            return "Cancellation rollback left managed artifact '"
                + Librova::Unicode::PathToUtf8(*resolvedPath)
                + "' on disk because "
                + reason;
        }

        CleanupEmptyParentsNoThrow(*resolvedPath, root);
        return std::nullopt;
    }
    catch (const std::exception& error)
    {
        const std::string reason = std::string{"Cleanup threw an exception: "} + error.what();
        LogRollbackCleanupIssueIfInitialized(normalizedManagedPath, reason);
        return "Cancellation rollback left managed artifact '"
            + Librova::Unicode::PathToUtf8(candidatePath)
            + "' on disk because "
            + reason;
    }
    catch (...)
    {
        const std::string reason = "Cleanup threw a non-standard exception.";
        LogRollbackCleanupIssueIfInitialized(normalizedManagedPath, reason);
        return "Cancellation rollback left managed artifact '"
            + Librova::Unicode::PathToUtf8(candidatePath)
            + "' on disk because "
            + reason;
    }
}

void LogRollbackFailureIfInitialized(
    const Librova::Domain::SBookId bookId,
    const std::exception& error)
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    Librova::Logging::Error(
        "Import cancellation rollback failed for book {}. Error='{}'.",
        bookId.Value,
        error.what());
}

} // namespace

namespace Librova::Application {

CImportRollbackService::CImportRollbackService(
    Librova::Domain::IBookRepository& bookRepository,
    std::filesystem::path libraryRoot)
    : m_bookRepository(bookRepository)
    , m_libraryRoot(std::move(libraryRoot))
{
}

SRollbackResult CImportRollbackService::RollbackImportedBooks(
    const std::vector<Librova::Domain::SBookId>& importedBookIds) const
{
    SRollbackResult rollbackResult;

    for (auto iterator = importedBookIds.rbegin(); iterator != importedBookIds.rend(); ++iterator)
    {
        std::optional<Librova::Domain::SBook> book;
        try
        {
            book = m_bookRepository.GetById(*iterator);
        }
        catch (const std::exception& error)
        {
            rollbackResult.RemainingBookIds.push_back(*iterator);
            rollbackResult.Warnings.push_back(
                "Cancellation rollback left book "
                + std::to_string(iterator->Value)
                + " in the library because catalog lookup failed: "
                + error.what());
            LogRollbackFailureIfInitialized(*iterator, error);
            continue;
        }

        try
        {
            m_bookRepository.Remove(*iterator);
        }
        catch (const std::exception& error)
        {
            rollbackResult.RemainingBookIds.push_back(*iterator);
            rollbackResult.Warnings.push_back(
                "Cancellation rollback left book "
                + std::to_string(iterator->Value)
                + " in the library because catalog removal failed: "
                + error.what());
            LogRollbackFailureIfInitialized(*iterator, error);
            continue;
        }

        if (book.has_value())
        {
            if (book->CoverPath.has_value())
            {
                if (const auto cleanupWarning = CleanupManagedPathNoThrow(m_libraryRoot, *book->CoverPath); cleanupWarning.has_value())
                {
                    rollbackResult.HasCleanupResidue = true;
                    rollbackResult.Warnings.push_back(*cleanupWarning);
                }
            }

            if (book->File.HasManagedPath())
            {
                if (const auto cleanupWarning = CleanupManagedPathNoThrow(m_libraryRoot, book->File.ManagedPath); cleanupWarning.has_value())
                {
                    rollbackResult.HasCleanupResidue = true;
                    rollbackResult.Warnings.push_back(*cleanupWarning);
                }
            }
        }
    }

    if (Librova::Logging::CLogging::IsInitialized())
    {
        if (rollbackResult.RemainingBookIds.empty())
        {
            Librova::Logging::Warn(
                "Rolled back {} previously imported book(s) after cancellation.",
                importedBookIds.size());
        }
        else
        {
            Librova::Logging::Warn(
                "Cancellation rollback left {} of {} imported book(s) in the library after catalog-removal failures.",
                rollbackResult.RemainingBookIds.size(),
                importedBookIds.size());
        }

        if (rollbackResult.HasCleanupResidue)
        {
            Librova::Logging::Warn(
                "Cancellation rollback left managed artifacts on disk for at least one imported book.");
        }
    }

    return rollbackResult;
}

} // namespace Librova::Application
