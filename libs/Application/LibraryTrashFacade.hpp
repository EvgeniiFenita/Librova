#pragma once

#include <filesystem>
#include <optional>

#include "Domain/BookRepository.hpp"
#include "Domain/ServiceContracts.hpp"

namespace Librova::Application {

struct STrashedBookResult
{
    Librova::Domain::SBookId BookId;
    std::filesystem::path TrashedBookPath;
    std::optional<std::filesystem::path> TrashedCoverPath;
};

class CLibraryTrashFacade final
{
public:
    CLibraryTrashFacade(
        Librova::Domain::IBookRepository& bookRepository,
        Librova::Domain::ITrashService& trashService,
        std::filesystem::path libraryRoot);

    [[nodiscard]] std::optional<STrashedBookResult> MoveBookToTrash(Librova::Domain::SBookId id) const;

private:
    [[nodiscard]] std::filesystem::path ResolveManagedSourcePath(const std::filesystem::path& managedPath) const;

    Librova::Domain::IBookRepository& m_bookRepository;
    Librova::Domain::ITrashService& m_trashService;
    std::filesystem::path m_libraryRoot;
};

} // namespace Librova::Application
