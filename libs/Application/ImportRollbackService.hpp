#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
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

using TRollbackProgressCallback = std::function<void(std::size_t rolledBack, std::size_t total)>;

class CImportRollbackService final
{
public:
    static constexpr auto kProgressReportInterval = std::chrono::milliseconds{250};

    CImportRollbackService(
        Librova::Domain::IBookRepository& bookRepository,
        std::filesystem::path libraryRoot);

    // progressCallback is called at least every kProgressReportInterval while
    // rollback work is making forward progress, and once more on completion.
    // May be nullptr.
    [[nodiscard]] SRollbackResult RollbackImportedBooks(
        const std::vector<Librova::Domain::SBookId>& importedBookIds,
        TRollbackProgressCallback progressCallback = nullptr) const;

    Librova::Domain::IBookRepository& m_bookRepository;
    std::filesystem::path m_libraryRoot;
};

} // namespace Librova::Application
