#include "ManagedPaths/ManagedPathSafety.hpp"

#include <Windows.h>

#include <stdexcept>
#include <system_error>

#include "Unicode/UnicodeConversion.hpp"

namespace Librova::ManagedPaths {

bool IsSafeRelativeManagedPath(const std::filesystem::path& path)
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

namespace {

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

[[nodiscard]] std::filesystem::path CanonicalizeExistingPath(
    const std::filesystem::path& path,
    std::string_view errorMessage)
{
    std::error_code errorCode;
    const auto canonicalPath = std::filesystem::weakly_canonical(path, errorCode);
    if (errorCode)
    {
        throw std::runtime_error(std::string{errorMessage});
    }

    return canonicalPath.lexically_normal();
}

[[nodiscard]] bool IsReparsePoint(const std::filesystem::path& path)
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        throw std::runtime_error("Failed to inspect filesystem attributes.");
    }

    return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

} // namespace

std::optional<std::filesystem::path> TryResolvePathWithinRoot(
    const std::filesystem::path& root,
    const std::filesystem::path& managedPath,
    const std::string_view unsafePathMessage,
    const std::string_view canonicalizationErrorMessage)
{
    const auto normalizedManagedPath = managedPath.lexically_normal();
    std::filesystem::path candidatePath;

    if (normalizedManagedPath.is_absolute())
    {
        candidatePath = normalizedManagedPath;
    }
    else
    {
        if (!IsSafeRelativeManagedPath(normalizedManagedPath))
        {
            throw std::runtime_error(std::string{unsafePathMessage});
        }

        candidatePath = (root / normalizedManagedPath).lexically_normal();
    }

    const auto canonicalRoot = CanonicalizeExistingPath(root, canonicalizationErrorMessage);
    const auto canonicalCandidate = CanonicalizeExistingPath(candidatePath, canonicalizationErrorMessage);
    if (!IsPathWithinRoot(canonicalRoot, canonicalCandidate))
    {
        throw std::runtime_error(std::string{unsafePathMessage});
    }

    if (!std::filesystem::exists(candidatePath))
    {
        return std::nullopt;
    }

    return canonicalCandidate;
}

std::filesystem::path ResolveExistingPathWithinRoot(
    const std::filesystem::path& root,
    const std::filesystem::path& managedPath,
    const std::string_view missingPathMessage,
    const std::string_view unsafePathMessage,
    const std::string_view canonicalizationErrorMessage)
{
    const auto resolvedPath = TryResolvePathWithinRoot(
        root,
        managedPath,
        unsafePathMessage,
        canonicalizationErrorMessage);
    if (!resolvedPath.has_value())
    {
        throw std::runtime_error(std::string{missingPathMessage});
    }

    return *resolvedPath;
}

void RemovePathRecursivelyWithinRoot(
    const std::filesystem::path& root,
    const std::filesystem::path& managedPath,
    const std::string_view missingPathMessage,
    const std::string_view unsafePathMessage,
    const std::string_view canonicalizationErrorMessage)
{
    const auto normalizedManagedPath = managedPath.lexically_normal();
    std::filesystem::path candidatePath;

    if (normalizedManagedPath.is_absolute())
    {
        candidatePath = normalizedManagedPath;
    }
    else
    {
        if (!IsSafeRelativeManagedPath(normalizedManagedPath))
        {
            throw std::runtime_error(std::string{unsafePathMessage});
        }

        candidatePath = (root / normalizedManagedPath).lexically_normal();
    }

    if (!std::filesystem::exists(candidatePath))
    {
        throw std::runtime_error(std::string{missingPathMessage});
    }

    const auto canonicalRoot = CanonicalizeExistingPath(root, canonicalizationErrorMessage);
    const auto candidateParentPath = candidatePath.parent_path().empty() ? candidatePath : candidatePath.parent_path();
    const auto canonicalParent = CanonicalizeExistingPath(candidateParentPath, canonicalizationErrorMessage);
    if (!IsPathWithinRoot(canonicalRoot, canonicalParent))
    {
        throw std::runtime_error(std::string{unsafePathMessage});
    }

    std::error_code errorCode;
    if (IsReparsePoint(candidatePath))
    {
        std::filesystem::remove(candidatePath, errorCode);
        if (errorCode)
        {
            throw std::runtime_error(
                "Failed to remove filesystem reparse point '" + Librova::Unicode::PathToUtf8(candidatePath) + "'.");
        }

        return;
    }

    const auto resolvedPath = CanonicalizeExistingPath(candidatePath, canonicalizationErrorMessage);
    if (!IsPathWithinRoot(canonicalRoot, resolvedPath))
    {
        std::filesystem::remove(candidatePath, errorCode);
        if (errorCode)
        {
            throw std::runtime_error(
                "Failed to remove managed path escaping through a reparse point '" + Librova::Unicode::PathToUtf8(candidatePath) + "'.");
        }

        return;
    }

    if (std::filesystem::is_directory(resolvedPath, errorCode))
    {
        std::filesystem::directory_iterator iterator(resolvedPath, errorCode);
        if (errorCode)
        {
            throw std::runtime_error(
                "Failed to enumerate directory '" + Librova::Unicode::PathToUtf8(resolvedPath) + "'.");
        }

        for (const auto& entry : iterator)
        {
            RemovePathRecursivelyWithinRoot(
                root,
                entry.path(),
                missingPathMessage,
                unsafePathMessage,
                canonicalizationErrorMessage);
        }
    }

    errorCode.clear();
    std::filesystem::remove(resolvedPath, errorCode);
    if (errorCode)
    {
        throw std::runtime_error(
            "Failed to remove managed path '" + Librova::Unicode::PathToUtf8(resolvedPath) + "'.");
    }
}

} // namespace Librova::ManagedPaths
