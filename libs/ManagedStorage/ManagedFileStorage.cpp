#include "ManagedStorage/ManagedFileStorage.hpp"

#include <filesystem>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

#include "Logging/Logging.hpp"
#include "ManagedFileEncoding/ManagedFileEncoding.hpp"
#include "StoragePlanning/ManagedLibraryLayout.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace Librova::ManagedStorage {
namespace {

void EnsureDirectory(const std::filesystem::path& path)
{
    std::error_code errorCode;
    std::filesystem::create_directories(path, errorCode);

    if (errorCode)
    {
        throw std::runtime_error(
            std::string{"Failed to create directory: "} + Librova::Unicode::PathToUtf8(path));
    }
}

void RemovePathNoThrow(const std::filesystem::path& path) noexcept
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

void RemoveEmptyDirectoryNoThrow(const std::filesystem::path& path) noexcept
{
    if (path.empty())
    {
        return;
    }

    std::error_code errorCode;
    std::filesystem::remove(path, errorCode);
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
    std::error_code errorCode;
    std::filesystem::rename(sourcePath, destinationPath, errorCode);

    if (errorCode)
    {
        throw std::runtime_error(
            std::string{"Failed to move file from "}
            + Librova::Unicode::PathToUtf8(sourcePath)
            + " to "
            + Librova::Unicode::PathToUtf8(destinationPath));
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
            + Librova::Unicode::PathToUtf8(destinationPath));
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

CManagedFileStorage::CManagedFileStorage(std::filesystem::path libraryRoot)
    : m_libraryRoot(std::move(libraryRoot))
{
}

Librova::Domain::SPreparedStorage CManagedFileStorage::PrepareImport(const Librova::Domain::SStoragePlan& plan)
{
    if (!plan.IsValid())
    {
        throw std::invalid_argument("Storage plan must contain a valid book id and source path.");
    }

    const Librova::StoragePlanning::SLibraryLayoutPaths layout = Librova::StoragePlanning::CManagedLibraryLayout::Build(m_libraryRoot);
    const std::filesystem::path stagingDirectory = Librova::StoragePlanning::CManagedLibraryLayout::GetStagingDirectory(m_libraryRoot, plan.BookId);
    const std::filesystem::path stagedBookPath = stagingDirectory / plan.SourcePath.filename();
    const std::filesystem::path finalBookPath =
        Librova::StoragePlanning::CManagedLibraryLayout::GetManagedBookPath(m_libraryRoot, plan.BookId, plan.Format, plan.StorageEncoding);

    EnsureDirectory(layout.BooksDirectory);
    EnsureDirectory(layout.CoversDirectory);
    EnsureDirectory(layout.TempDirectory);

    RemovePathNoThrow(stagingDirectory);
    EnsureDirectory(stagingDirectory);

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

            const std::filesystem::path finalCoverPath =
                Librova::StoragePlanning::CManagedLibraryLayout::GetCoverPath(m_libraryRoot, plan.BookId, extension);

            preparedStorage.StagedCoverPath = stagedCoverPath;
            preparedStorage.FinalCoverPath = finalCoverPath;
            preparedStorage.RelativeCoverPath = std::filesystem::relative(finalCoverPath, m_libraryRoot);
        }

        return preparedStorage;
    }
    catch (...)
    {
        RemovePathNoThrow(stagingDirectory);
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
        EnsureDirectory(preparedStorage.FinalBookPath.parent_path());
        MoveFile(preparedStorage.StagedBookPath, preparedStorage.FinalBookPath);
        movedBook = true;

        if (preparedStorage.StagedCoverPath.has_value())
        {
            if (!preparedStorage.FinalCoverPath.has_value())
            {
                throw std::invalid_argument("Prepared storage must contain a final cover path when a cover is staged.");
            }

            EnsureDirectory(preparedStorage.FinalCoverPath->parent_path());
            MoveFile(*preparedStorage.StagedCoverPath, *preparedStorage.FinalCoverPath);
            movedCover = true;
        }

        RemovePathNoThrow(preparedStorage.StagedBookPath.parent_path());
    }
    catch (...)
    {
        const auto originalError = std::current_exception();
        std::vector<std::string> rollbackDiagnostics;

        if (movedCover && preparedStorage.StagedCoverPath.has_value() && preparedStorage.FinalCoverPath.has_value())
        {
            try
            {
                EnsureDirectory(preparedStorage.StagedCoverPath->parent_path());
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
            }
        }

        if (movedBook)
        {
            try
            {
                EnsureDirectory(preparedStorage.StagedBookPath.parent_path());
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
    const std::filesystem::path managedBookDirectory = preparedStorage.FinalBookPath.parent_path();

    RemovePathNoThrow(preparedStorage.StagedBookPath);
    RemovePathNoThrow(preparedStorage.FinalBookPath);

    if (preparedStorage.StagedCoverPath.has_value())
    {
        RemovePathNoThrow(*preparedStorage.StagedCoverPath);
    }

    if (preparedStorage.FinalCoverPath.has_value())
    {
        RemovePathNoThrow(*preparedStorage.FinalCoverPath);
    }

    RemoveEmptyDirectoryNoThrow(stagingDirectory);
    RemoveEmptyDirectoryNoThrow(managedBookDirectory);
}

} // namespace Librova::ManagedStorage
