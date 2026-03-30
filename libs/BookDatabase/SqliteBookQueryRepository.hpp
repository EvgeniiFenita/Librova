#pragma once

#include <filesystem>
#include <vector>

#include "Domain/BookRepository.hpp"

namespace LibriFlow::BookDatabase {

class CSqliteBookQueryRepository final : public LibriFlow::Domain::IBookQueryRepository
{
public:
    explicit CSqliteBookQueryRepository(std::filesystem::path databasePath);

    [[nodiscard]] std::vector<LibriFlow::Domain::SBook> Search(const LibriFlow::Domain::SSearchQuery& query) const override;
    [[nodiscard]] std::vector<LibriFlow::Domain::SDuplicateMatch> FindDuplicates(const LibriFlow::Domain::SCandidateBook& candidate) const override;

private:
    std::filesystem::path m_databasePath;
};

} // namespace LibriFlow::BookDatabase
