#include "Application/LibraryExportFacade.hpp"

#include <system_error>
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

[[nodiscard]] std::filesystem::path CanonicalizeExistingPath(const std::filesystem::path& path)
{
    std::error_code errorCode;
    const auto canonicalPath = std::filesystem::weakly_canonical(path, errorCode);
    if (errorCode)
    {
        throw std::runtime_error("Managed source path could not be canonicalized.");
    }

    return canonicalPath.lexically_normal();
}

} // namespace

CLibraryExportFacade::CLibraryExportFacade(
    const Librova::Domain::IBookRepository& bookRepository,
    std::filesystem::path libraryRoot)
    : m_bookRepository(bookRepository)
    , m_libraryRoot(std::move(libraryRoot))
{
}

std::optional<std::filesystem::path> CLibraryExportFacade::ExportBook(
    const Librova::Domain::SBookId id,
    const std::filesystem::path& destinationPath) const
{
    if (destinationPath.empty())
    {
        throw std::invalid_argument("Export destination path cannot be empty.");
    }

    if (!destinationPath.is_absolute())
    {
        throw std::invalid_argument("Export destination path must be absolute.");
    }

    const auto book = m_bookRepository.GetById(id);
    if (!book.has_value())
    {
        return std::nullopt;
    }

    if (!book->File.HasManagedPath())
    {
        throw std::runtime_error("Book does not have a managed file path.");
    }

    const auto sourcePath = ResolveManagedSourcePath(book->File.ManagedPath);

    if (std::filesystem::is_directory(destinationPath))
    {
        throw std::invalid_argument("Export destination path must point to a file.");
    }

    if (!destinationPath.parent_path().empty())
    {
        std::filesystem::create_directories(destinationPath.parent_path());
    }

    std::filesystem::copy_file(
        sourcePath,
        destinationPath,
        std::filesystem::copy_options::overwrite_existing);

    return destinationPath;
}

std::filesystem::path CLibraryExportFacade::ResolveManagedSourcePath(const std::filesystem::path& managedPath) const
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
            throw std::runtime_error("Managed source path is unsafe.");
        }

        candidatePath = (m_libraryRoot / normalizedManagedPath).lexically_normal();
    }

    if (!std::filesystem::exists(candidatePath))
    {
        throw std::runtime_error("Managed source file does not exist.");
    }

    const auto canonicalRoot = CanonicalizeExistingPath(m_libraryRoot);
    const auto canonicalCandidate = CanonicalizeExistingPath(candidatePath);
    if (!IsPathWithinRoot(canonicalRoot, canonicalCandidate))
    {
        throw std::runtime_error("Managed source path is unsafe.");
    }

    return canonicalCandidate;
}

} // namespace Librova::Application
