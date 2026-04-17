#include "CoreHost/LibraryBootstrap.hpp"

#include <stdexcept>
#include <system_error>

#include "StoragePlanning/ManagedLibraryLayout.hpp"

namespace Librova::CoreHost {
namespace {

void EnsureDirectoryExists(const std::filesystem::path& path, const char* label)
{
    if (!std::filesystem::is_directory(path))
    {
        throw std::runtime_error(std::string{"Managed library is missing required directory: "} + label + ".");
    }
}

void ValidateExistingLibraryRoot(const std::filesystem::path& libraryRoot)
{
    if (!std::filesystem::is_directory(libraryRoot))
    {
        throw std::runtime_error("Managed library root does not exist.");
    }

    const auto layout = Librova::StoragePlanning::CManagedLibraryLayout::Build(libraryRoot);
    EnsureDirectoryExists(layout.DatabaseDirectory, "Database");
    EnsureDirectoryExists(layout.BooksDirectory, "Books");
    EnsureDirectoryExists(layout.CoversDirectory, "Covers");
    EnsureDirectoryExists(layout.LogsDirectory, "Logs");
    EnsureDirectoryExists(layout.TrashDirectory, "Trash");

    if (!std::filesystem::is_regular_file(layout.DatabaseDirectory / "librova.db"))
    {
        throw std::runtime_error("Managed library database file is missing.");
    }
}

void PrepareNewLibraryRoot(const std::filesystem::path& libraryRoot)
{
    if (std::filesystem::exists(libraryRoot))
    {
        std::error_code errorCode;
        const bool isEmpty = std::filesystem::is_empty(libraryRoot, errorCode);
        if (errorCode)
        {
            throw std::runtime_error("Failed to inspect target library root before creation.");
        }

        if (!isEmpty)
        {
            throw std::runtime_error("New library root must be empty.");
        }
    }

    const auto layout = Librova::StoragePlanning::CManagedLibraryLayout::Build(libraryRoot);
    std::filesystem::create_directories(layout.Root);
    std::filesystem::create_directories(layout.DatabaseDirectory);
    std::filesystem::create_directories(layout.BooksDirectory);
    std::filesystem::create_directories(layout.CoversDirectory);
    std::filesystem::create_directories(layout.TrashDirectory);
    std::filesystem::create_directories(layout.LogsDirectory);
}

} // namespace

void CLibraryBootstrap::PrepareLibraryRoot(
    const std::filesystem::path& libraryRoot,
    const ELibraryOpenMode libraryOpenMode)
{
    switch (libraryOpenMode)
    {
    case ELibraryOpenMode::OpenExisting:
        ValidateExistingLibraryRoot(libraryRoot);
        break;
    case ELibraryOpenMode::CreateNew:
        PrepareNewLibraryRoot(libraryRoot);
        break;
    }
}

} // namespace Librova::CoreHost
