#include "ManagedPaths/ManagedPathSafety.hpp"

#include <stdexcept>
#include <system_error>

namespace Librova::ManagedPaths {
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

} // namespace

std::filesystem::path ResolveExistingPathWithinRoot(
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
    const auto canonicalCandidate = CanonicalizeExistingPath(candidatePath, canonicalizationErrorMessage);
    if (!IsPathWithinRoot(canonicalRoot, canonicalCandidate))
    {
        throw std::runtime_error(std::string{unsafePathMessage});
    }

    return canonicalCandidate;
}

} // namespace Librova::ManagedPaths
