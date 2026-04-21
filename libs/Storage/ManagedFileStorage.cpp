#include "Storage/ManagedFileStorage.hpp"

#include <filesystem>
#include <exception>
#include <format>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include "Foundation/FileSystemUtils.hpp"
#include "Foundation/Logging.hpp"
#include "Storage/ManagedPathSafety.hpp"
#include "Storage/ManagedFileEncoding.hpp"
#include "Storage/ManagedLibraryLayout.hpp"
#include "Foundation/UnicodeConversion.hpp"

namespace Librova::ManagedStorage {
namespace {

void RemovePathWithWarningNoThrow(const std::filesystem::path& path) noexcept
{
    if (path.empty())
    {
        return;
    }

    std::error_code errorCode;
    std::filesystem::remove_all(path, errorCode);
    if (errorCode && Librova::Logging::CLogging::IsInitialized())
    {
        try
        {
            Librova::Logging::Warn(
                "Best-effort managed-storage cleanup failed. Path='{}' Error='{}'.",
                Librova::Unicode::PathToUtf8(path),
                errorCode.message());
        }
        catch (...)
        {
        }
    }
}

void LogRestoreFailureIfInitialized(
    std::string_view label,
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& destinationPath,
    const std::exception& error) noexcept
{
    if (!Librova::Logging::CLogging::IsInitialized())
    {
        return;
    }

    try
    {
        Librova::Logging::Warn(
            "Managed-storage rollback could not restore {} from '{}' to '{}'. Error='{}'.",
            label,
            Librova::Unicode::PathToUtf8(sourcePath),
            Librova::Unicode::PathToUtf8(destinationPath),
            error.what());
    }
    catch (...)
    {
    }
}

[[nodiscard]] std::string DescribeException(const std::exception_ptr& error)
{
    if (error == nullptr)
    {
        return "Unknown managed-storage commit failure.";
    }

    try
    {
        std::rethrow_exception(error);
    }
    catch (const std::exception& ex)
    {
        return ex.what();
    }
    catch (...)
    {
        return "Unknown managed-storage commit failure.";
    }
}

[[nodiscard]] std::string BuildRollbackFailureMessage(
    const std::string& originalError,
    const std::vector<std::string>& rollbackDiagnostics)
{
    std::string message = "Managed storage commit failed: " + originalError;

    for (const auto& diagnostic : rollbackDiagnostics)
    {
        if (diagnostic.empty())
        {
            continue;
        }

        message += " ";
        message += diagnostic;
    }

    return message;
}

void MoveFile(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationPath)
{
    std::error_code renameError;
    std::filesystem::rename(sourcePath, destinationPath, renameError);

    if (!renameError)
    {
        return;
    }

    // std::filesystem::rename (MoveFileExW) fails across filesystem boundaries,
    // e.g. when the staging root is on a local drive and the library is on NAS.
    // Fall back to copy + delete so the import succeeds in that configuration.
    if (Librova::Logging::CLogging::IsInitialized())
    {
        try
        {
            Librova::Logging::Info(
                "MoveFile: atomic rename failed ({}), falling back to copy+delete. src='{}' dst='{}'.",
                renameError.message(),
                Librova::Unicode::PathToUtf8(sourcePath),
                Librova::Unicode::PathToUtf8(destinationPath));
        }
        catch (...)
        {
        }
    }

    std::error_code copyError;
    const bool copied = std::filesystem::copy_file(
        sourcePath,
        destinationPath,
        std::filesystem::copy_options::overwrite_existing,
        copyError);

    if (!copied || copyError)
    {
        throw std::runtime_error(
            std::string{"Failed to move file from "}
            + Librova::Unicode::PathToUtf8(sourcePath)
            + " to "
            + Librova::Unicode::PathToUtf8(destinationPath)
            + ": rename failed ("
            + renameError.message()
            + "), copy fallback also failed ("
            + copyError.message()
            + ")");
    }

    std::error_code removeError;
    std::filesystem::remove(sourcePath, removeError);
    if (removeError && Librova::Logging::CLogging::IsInitialized())
    {
        try
        {
            Librova::Logging::Warn(
                "MoveFile: copy succeeded but source removal failed — staging cleanup will handle it. src='{}' err='{}'.",
                Librova::Unicode::PathToUtf8(sourcePath),
                removeError.message());
        }
        catch (...)
        {
        }
    }
}

void CopyFile(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationPath)
{
    std::error_code errorCode;
    const bool copied = std::filesystem::copy_file(
        sourcePath,
        destinationPath,
        std::filesystem::copy_options::overwrite_existing,
        errorCode);

    if (!copied || errorCode)
    {
        throw std::runtime_error(
            std::string{"Failed to copy file from "}
            + Librova::Unicode::PathToUtf8(sourcePath)
            + " to "
            + Librova::Unicode::PathToUtf8(destinationPath)
            + ": " + errorCode.message());
    }
}

std::filesystem::path BuildStagedCoverPath(
    const std::filesystem::path& stagingDirectory,
    const std::filesystem::path& coverSourcePath)
{
    const std::filesystem::path extension = coverSourcePath.extension();

    if (extension.empty())
    {
        throw std::invalid_argument("Cover source path must have an extension.");
    }

    return stagingDirectory / std::filesystem::path{"cover"}.replace_extension(extension);
}

} // namespace

CManagedFileStorage::CManagedFileStorage(
    std::filesystem::path libraryRoot,
    std::filesystem::path stagingRoot)
    : m_libraryRoot(std::move(libraryRoot))
    , m_stagingRoot(std::move(stagingRoot))
{
    if (m_stagingRoot.empty())
    {
        throw std::invalid_argument("Managed storage staging root must not be empty.");
    }
}

Librova::Domain::SPreparedStorage CManagedFileStorage::PrepareImport(const Librova::Domain::SStoragePlan& plan)
{
    if (!plan.IsValid())
    {
        throw std::invalid_argument("Storage plan must contain a valid book id and source path.");
    }

    const Librova::StoragePlanning::SLibraryLayoutPaths layout = Librova::StoragePlanning::CManagedLibraryLayout::Build(m_libraryRoot);
    const std::string bookFolderName = Librova::StoragePlanning::CManagedLibraryLayout::GetBookFolderName(plan.BookId);
    const std::filesystem::path stagingDirectory = m_stagingRoot / bookFolderName;
    const std::filesystem::path stagedBookPath = stagingDirectory / plan.SourcePath.filename();
    const std::filesystem::path finalBookPath = Librova::StoragePlanning::CManagedLibraryLayout::GetManagedBookPath(
        m_libraryRoot,
        plan.BookId,
        plan.Format,
        plan.StorageEncoding);

    std::call_once(m_rootDirsEnsuredOnce, [&] {
        Librova::Foundation::EnsureDirectory(layout.ObjectsDirectory);
        Librova::Foundation::EnsureDirectory(m_stagingRoot);
    });

    RemovePathWithWarningNoThrow(stagingDirectory);
    Librova::Foundation::EnsureDirectory(stagingDirectory);

    try
    {
        Librova::ManagedFileEncoding::EncodeFileToPath(
            plan.SourcePath,
            plan.StorageEncoding,
            stagedBookPath);

        Librova::Domain::SPreparedStorage preparedStorage{
            .StagedBookPath = stagedBookPath,
            .FinalBookPath = finalBookPath,
            .RelativeBookPath = std::filesystem::relative(finalBookPath, m_libraryRoot)
        };

        if (plan.CoverSourcePath.has_value())
        {
            const std::filesystem::path stagedCoverPath = BuildStagedCoverPath(stagingDirectory, *plan.CoverSourcePath);
            const std::string extension = stagedCoverPath.extension().string();

            CopyFile(*plan.CoverSourcePath, stagedCoverPath);

            if (extension.empty())
                throw std::invalid_argument("Cover extension must not be empty.");
            const std::string normalizedExt = extension.front() == '.' ? extension.substr(1) : extension;
            const std::filesystem::path finalCoverPath =
                Librova::StoragePlanning::CManagedLibraryLayout::GetCoverPath(m_libraryRoot, plan.BookId, normalizedExt);

            preparedStorage.StagedCoverPath = stagedCoverPath;
            preparedStorage.FinalCoverPath = finalCoverPath;
            preparedStorage.RelativeCoverPath = std::filesystem::relative(finalCoverPath, m_libraryRoot);
        }

        return preparedStorage;
    }
    catch (...)
    {
        RemovePathWithWarningNoThrow(stagingDirectory);
        throw;
    }
}

void CManagedFileStorage::CommitImport(const Librova::Domain::SPreparedStorage& preparedStorage)
{
    if (!preparedStorage.HasStagedBook())
    {
        throw std::invalid_argument("Prepared storage must contain a staged book.");
    }

    bool movedBook = false;
    bool movedCover = false;

    try
    {
        Librova::Foundation::EnsureDirectory(preparedStorage.FinalBookPath.parent_path());
        MoveFile(preparedStorage.StagedBookPath, preparedStorage.FinalBookPath);
        movedBook = true;

        if (preparedStorage.StagedCoverPath.has_value())
        {
            if (!preparedStorage.FinalCoverPath.has_value())
            {
                throw std::invalid_argument("Prepared storage must contain a final cover path when a cover is staged.");
            }

            Librova::Foundation::EnsureDirectory(preparedStorage.FinalCoverPath->parent_path());
            MoveFile(*preparedStorage.StagedCoverPath, *preparedStorage.FinalCoverPath);
            movedCover = true;
        }

        RemovePathWithWarningNoThrow(preparedStorage.StagedBookPath.parent_path());
    }
    catch (...)
    {
        const auto originalError = std::current_exception();
        std::vector<std::string> rollbackDiagnostics;

        if (movedCover && preparedStorage.StagedCoverPath.has_value() && preparedStorage.FinalCoverPath.has_value())
        {
            try
            {
                Librova::Foundation::EnsureDirectory(preparedStorage.StagedCoverPath->parent_path());
                MoveFile(*preparedStorage.FinalCoverPath, *preparedStorage.StagedCoverPath);
            }
            catch (const std::exception& error)
            {
                rollbackDiagnostics.push_back(
                    "Rollback could not restore the managed cover from '"
                    + Librova::Unicode::PathToUtf8(*preparedStorage.FinalCoverPath)
                    + "' back to staging path '"
                    + Librova::Unicode::PathToUtf8(*preparedStorage.StagedCoverPath)
                    + "'.");
                LogRestoreFailureIfInitialized(
                    "cover",
                    *preparedStorage.FinalCoverPath,
                    *preparedStorage.StagedCoverPath,
                    error);
            }
            catch (...)
            {
                if (Librova::Logging::CLogging::IsInitialized())
                {
                    Librova::Logging::Warn(
                        "Commit rollback: unexpected non-std exception during cover restore.");
                }
            }
        }

        if (movedBook)
        {
            try
            {
                Librova::Foundation::EnsureDirectory(preparedStorage.StagedBookPath.parent_path());
                MoveFile(preparedStorage.FinalBookPath, preparedStorage.StagedBookPath);
            }
            catch (const std::exception& error)
            {
                rollbackDiagnostics.push_back(
                    "Rollback could not restore the managed book from '"
                    + Librova::Unicode::PathToUtf8(preparedStorage.FinalBookPath)
                    + "' back to staging path '"
                    + Librova::Unicode::PathToUtf8(preparedStorage.StagedBookPath)
                    + "'.");
                LogRestoreFailureIfInitialized(
                    "book",
                    preparedStorage.FinalBookPath,
                    preparedStorage.StagedBookPath,
                    error);
            }
            catch (...)
            {
                if (Librova::Logging::CLogging::IsInitialized())
                {
                    Librova::Logging::Warn(
                        "Commit rollback: unexpected non-std exception during book restore.");
                }
            }
        }

        if (!rollbackDiagnostics.empty())
        {
            throw std::runtime_error(
                BuildRollbackFailureMessage(
                    DescribeException(originalError),
                    rollbackDiagnostics));
        }

        std::rethrow_exception(originalError);
    }
}

void CManagedFileStorage::RollbackImport(const Librova::Domain::SPreparedStorage& preparedStorage) noexcept
{
    const std::filesystem::path stagingDirectory = preparedStorage.StagedBookPath.parent_path();
    const std::filesystem::path managedObjectDirectory = preparedStorage.FinalBookPath.parent_path();
    const auto layout = Librova::StoragePlanning::CManagedLibraryLayout::Build(m_libraryRoot);

    RemovePathWithWarningNoThrow(preparedStorage.StagedBookPath);
    RemovePathWithWarningNoThrow(preparedStorage.FinalBookPath);

    if (preparedStorage.StagedCoverPath.has_value())
    {
        RemovePathWithWarningNoThrow(*preparedStorage.StagedCoverPath);
    }

    if (preparedStorage.FinalCoverPath.has_value())
    {
        RemovePathWithWarningNoThrow(*preparedStorage.FinalCoverPath);
    }

    (void)Librova::ManagedPaths::CleanupEmptyDirectoriesUpTo(stagingDirectory, m_stagingRoot);
    (void)Librova::ManagedPaths::CleanupEmptyDirectoriesUpTo(managedObjectDirectory, layout.ObjectsDirectory);
}

} // namespace Librova::ManagedStorage
