#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Domain/BookRepository.hpp"

namespace Librova::Application {

struct SRollbackResult
{
    std::vector<Librova::Domain::SBookId> RemainingBookIds;
    std::vector<std::string> Warnings;
    bool HasCleanupResidue = false;
};

class CImportRollbackService final
{
public:
    CImportRollbackService(
        Librova::Domain::IBookRepository& bookRepository,
        std::filesystem::path libraryRoot);

    [[nodiscard]] SRollbackResult RollbackImportedBooks(
        const std::vector<Librova::Domain::SBookId>& importedBookIds) const;

private:
    Librova::Domain::IBookRepository& m_bookRepository;
    std::filesystem::path m_libraryRoot;
};

} // namespace Librova::Application
