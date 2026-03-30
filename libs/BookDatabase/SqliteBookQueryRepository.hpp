#pragma once

#include <filesystem>
#include <vector>

#include "Domain/BookRepository.hpp"

namespace Librova::BookDatabase {

class CSqliteBookQueryRepository final : public Librova::Domain::IBookQueryRepository
{
public:
    explicit CSqliteBookQueryRepository(std::filesystem::path databasePath);

    [[nodiscard]] std::vector<Librova::Domain::SBook> Search(const Librova::Domain::SSearchQuery& query) const override;
    [[nodiscard]] std::vector<Librova::Domain::SDuplicateMatch> FindDuplicates(const Librova::Domain::SCandidateBook& candidate) const override;

private:
    std::filesystem::path m_databasePath;
};

} // namespace Librova::BookDatabase
