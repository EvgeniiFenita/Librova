#include "ManagedTrash/ManagedTrashService.hpp"

#include <system_error>
#include <stdexcept>
#include <string>

#include "StoragePlanning/ManagedLibraryLayout.hpp"

namespace Librova::ManagedTrash {
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

[[nodiscard]] std::filesystem::path CanonicalizeExistingPath(const std::filesystem::path& path)
{
    std::error_code errorCode;
    const auto canonicalPath = std::filesystem::weakly_canonical(path, errorCode);
    if (errorCode)
    {
        throw std::runtime_error("Managed trash path could not be canonicalized.");
    }

    return canonicalPath.lexically_normal();
}

void EnsureDirectory(const std::filesystem::path& path)
{
    std::error_code errorCode;
    std::filesystem::create_directories(path, errorCode);

    if (errorCode)
    {
        throw std::runtime_error(std::string{"Failed to create directory: "} + path.string());
    }
}

void MoveFile(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationPath)
{
    std::error_code errorCode;
    std::filesystem::rename(sourcePath, destinationPath, errorCode);

    if (errorCode)
    {
        throw std::runtime_error(
            std::string{"Failed to move file from "} + sourcePath.string() + " to " + destinationPath.string());
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
    const auto normalizedPath = path.lexically_normal();
    std::filesystem::path candidatePath;

    if (normalizedPath.is_absolute())
    {
        candidatePath = normalizedPath;
    }
    else
    {
        if (!IsSafeRelativeManagedPath(normalizedPath))
        {
            throw std::runtime_error("Managed trash path is unsafe.");
        }

        candidatePath = (m_libraryRoot / normalizedPath).lexically_normal();
    }

    if (!std::filesystem::exists(candidatePath))
    {
        throw std::runtime_error("Managed source path does not exist.");
    }

    const auto canonicalRoot = CanonicalizeExistingPath(m_libraryRoot);
    const auto canonicalCandidate = CanonicalizeExistingPath(candidatePath);
    if (!IsPathWithinRoot(canonicalRoot, canonicalCandidate))
    {
        throw std::runtime_error("Managed trash path is unsafe.");
    }

    return canonicalCandidate;
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
