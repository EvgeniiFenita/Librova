#pragma once

#include <filesystem>
#include <optional>

#include "Domain/BookRepository.hpp"

namespace Librova::Application {

class CLibraryExportFacade final
{
public:
    CLibraryExportFacade(
        const Librova::Domain::IBookRepository& bookRepository,
        std::filesystem::path libraryRoot);

    [[nodiscard]] std::optional<std::filesystem::path> ExportBook(
        Librova::Domain::SBookId id,
        const std::filesystem::path& destinationPath) const;

private:
    [[nodiscard]] std::filesystem::path ResolveManagedSourcePath(const std::filesystem::path& managedPath) const;

    const Librova::Domain::IBookRepository& m_bookRepository;
    std::filesystem::path m_libraryRoot;
};

} // namespace Librova::Application
