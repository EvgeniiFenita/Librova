#include "CoreHost/LibraryBootstrap.hpp"

#include "StoragePlanning/ManagedLibraryLayout.hpp"

namespace Librova::CoreHost {

void CLibraryBootstrap::PrepareLibraryRoot(const std::filesystem::path& libraryRoot)
{
    const auto layout = Librova::StoragePlanning::CManagedLibraryLayout::Build(libraryRoot);
    std::filesystem::create_directories(layout.Root);
    std::filesystem::create_directories(layout.DatabaseDirectory);
    std::filesystem::create_directories(layout.BooksDirectory);
    std::filesystem::create_directories(layout.CoversDirectory);
    std::filesystem::remove_all(layout.TempDirectory);
    std::filesystem::create_directories(layout.TempDirectory);
    std::filesystem::create_directories(layout.TrashDirectory);
    std::filesystem::create_directories(layout.LogsDirectory);
}

} // namespace Librova::CoreHost
