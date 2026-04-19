#include "Storage/ManagedTrashService.hpp"

#include <stdexcept>
#include <string>

#include "Foundation/FileSystemUtils.hpp"
#include "Storage/ManagedPathSafety.hpp"
#include "Storage/ManagedLibraryLayout.hpp"
#include "Foundation/UnicodeConversion.hpp"

namespace Librova::ManagedTrash {
namespace {

[[nodiscard]] std::error_code DefaultMoveOperation(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& destinationPath)
{
    std::error_code errorCode;
    std::filesystem::rename(sourcePath, destinationPath, errorCode);
    return errorCode;
}

void MoveFile(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& destinationPath,
    const CManagedTrashService::TMoveOperation& moveOperation)
{
    const auto errorCode = moveOperation(sourcePath, destinationPath);

    if (errorCode)
    {
        throw std::runtime_error(
            std::string{"Failed to move file from "}
            + Librova::Unicode::PathToUtf8(sourcePath)
            + " to "
            + Librova::Unicode::PathToUtf8(destinationPath));
    }
}

[[nodiscard]] std::filesystem::path BuildCollisionCandidatePath(const std::filesystem::path& path, const int index)
{
    if (index == 0)
    {
        return path;
    }

    const auto parentPath = path.parent_path();
    const auto stem = path.stem().string();
    const auto extension = path.extension().string();
    return parentPath / (stem + ".trashed-" + std::to_string(index) + extension);
}

[[nodiscard]] std::filesystem::path MoveFileToCollisionSafeDestination(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& destinationTemplate,
    const CManagedTrashService::TMoveOperation& moveOperation)
{
    for (int index = 1; index < 1000; ++index)
    {
        const auto candidate = BuildCollisionCandidatePath(destinationTemplate, index - 1);
        const auto errorCode = moveOperation(sourcePath, candidate);
        if (!errorCode)
        {
            return candidate;
        }

        std::error_code existsError;
        if (std::filesystem::exists(candidate, existsError) && !existsError)
        {
            continue;
        }

        throw std::runtime_error(
            std::string{"Failed to move file from "}
            + Librova::Unicode::PathToUtf8(sourcePath)
            + " to "
            + Librova::Unicode::PathToUtf8(candidate));
    }

    throw std::runtime_error("Failed to find a collision-safe trash path.");
}

} // namespace

CManagedTrashService::CManagedTrashService(
    std::filesystem::path libraryRoot,
    TMoveOperation moveOperation)
    : m_libraryRoot(std::move(libraryRoot))
    , m_moveOperation(moveOperation ? std::move(moveOperation) : &DefaultMoveOperation)
{
}

std::filesystem::path CManagedTrashService::MoveToTrash(const std::filesystem::path& path)
{
    const auto sourcePath = ResolveManagedPath(path);
    if (!std::filesystem::exists(sourcePath))
    {
        throw std::runtime_error("Managed source path does not exist.");
    }

    const auto destinationPath = BuildTrashDestination(sourcePath);
    Librova::Foundation::EnsureDirectory(destinationPath.parent_path());
    const auto movedPath = MoveFileToCollisionSafeDestination(sourcePath, destinationPath, m_moveOperation);
    const auto objectsRoot = Librova::StoragePlanning::CManagedLibraryLayout::Build(m_libraryRoot).ObjectsDirectory;
    (void)Librova::ManagedPaths::CleanupEmptyDirectoriesUpTo(sourcePath.parent_path(), objectsRoot);
    return movedPath;
}

void CManagedTrashService::RestoreFromTrash(
    const std::filesystem::path& trashedPath,
    const std::filesystem::path& destinationPath)
{
    if (trashedPath.empty() || destinationPath.empty())
    {
        throw std::invalid_argument("Trash restore paths must not be empty.");
    }

    Librova::Foundation::EnsureDirectory(destinationPath.parent_path());
    MoveFile(trashedPath, destinationPath, m_moveOperation);
    const auto trashObjectsRoot =
        Librova::StoragePlanning::CManagedLibraryLayout::Build(m_libraryRoot).TrashDirectory / "Objects";
    (void)Librova::ManagedPaths::CleanupEmptyDirectoriesUpTo(trashedPath.parent_path(), trashObjectsRoot);
}

std::filesystem::path CManagedTrashService::ResolveManagedPath(const std::filesystem::path& path) const
{
    return Librova::ManagedPaths::ResolveExistingPathWithinRoot(
        m_libraryRoot,
        path,
        "Managed source path does not exist.",
        "Managed trash path is unsafe.",
        "Managed trash path could not be canonicalized.");
}

std::filesystem::path CManagedTrashService::BuildTrashDestination(const std::filesystem::path& sourcePath) const
{
    const auto relativePath = sourcePath.lexically_relative(m_libraryRoot);
    if (relativePath.empty() || relativePath == ".")
    {
        throw std::runtime_error("Managed trash source path is outside the library root.");
    }

    for (const auto& part : relativePath)
    {
        if (part == "..")
        {
            throw std::runtime_error("Managed trash source path is outside the library root.");
        }
    }

    const auto trashRoot = Librova::StoragePlanning::CManagedLibraryLayout::Build(m_libraryRoot).TrashDirectory;
    return (trashRoot / relativePath).lexically_normal();
}

} // namespace Librova::ManagedTrash
