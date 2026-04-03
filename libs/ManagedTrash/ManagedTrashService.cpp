#include "ManagedTrash/ManagedTrashService.hpp"

#include <system_error>
#include <stdexcept>
#include <string>

#include "ManagedPaths/ManagedPathSafety.hpp"
#include "StoragePlanning/ManagedLibraryLayout.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace Librova::ManagedTrash {
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

void CleanupEmptyParentDirectory(const std::filesystem::path& path) noexcept
{
    const auto parentPath = path.parent_path();
    if (parentPath.empty())
    {
        return;
    }

    std::error_code errorCode;
    std::filesystem::remove(parentPath, errorCode);
}

[[nodiscard]] std::filesystem::path BuildCollisionSafePath(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path))
    {
        return path;
    }

    const auto parentPath = path.parent_path();
    const auto stem = path.stem().string();
    const auto extension = path.extension().string();

    for (int index = 1; index < 1000; ++index)
    {
        const auto candidate = parentPath / (stem + ".trashed-" + std::to_string(index) + extension);
        if (!std::filesystem::exists(candidate))
        {
            return candidate;
        }
    }

    throw std::runtime_error("Failed to find a collision-safe trash path.");
}

} // namespace

CManagedTrashService::CManagedTrashService(std::filesystem::path libraryRoot)
    : m_libraryRoot(std::move(libraryRoot))
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
    EnsureDirectory(destinationPath.parent_path());
    MoveFile(sourcePath, destinationPath);
    CleanupEmptyParentDirectory(sourcePath);
    return destinationPath;
}

void CManagedTrashService::RestoreFromTrash(
    const std::filesystem::path& trashedPath,
    const std::filesystem::path& destinationPath)
{
    if (trashedPath.empty() || destinationPath.empty())
    {
        throw std::invalid_argument("Trash restore paths must not be empty.");
    }

    EnsureDirectory(destinationPath.parent_path());
    MoveFile(trashedPath, destinationPath);
    CleanupEmptyParentDirectory(trashedPath);
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
    return BuildCollisionSafePath((trashRoot / relativePath).lexically_normal());
}

} // namespace Librova::ManagedTrash
