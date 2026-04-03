#pragma once

#include <filesystem>
#include <optional>

#include "Domain/BookRepository.hpp"
#include "Domain/ServiceContracts.hpp"

namespace Librova::Application {

enum class ETrashDestination
{
    ManagedTrash,
    RecycleBin
};

struct STrashedBookResult
{
    Librova::Domain::SBookId BookId;
    ETrashDestination Destination = ETrashDestination::ManagedTrash;
};

class CLibraryTrashFacade final
{
public:
    CLibraryTrashFacade(
        Librova::Domain::IBookRepository& bookRepository,
        Librova::Domain::ITrashService& trashService,
        std::filesystem::path libraryRoot,
        Librova::Domain::IRecycleBinService* recycleBinService = nullptr);

    [[nodiscard]] std::optional<STrashedBookResult> MoveBookToTrash(Librova::Domain::SBookId id) const;

private:
    [[nodiscard]] std::filesystem::path ResolveManagedSourcePath(const std::filesystem::path& managedPath) const;

    Librova::Domain::IBookRepository& m_bookRepository;
    Librova::Domain::ITrashService& m_trashService;
    std::filesystem::path m_libraryRoot;
    Librova::Domain::IRecycleBinService* m_recycleBinService;
};

} // namespace Librova::Application
