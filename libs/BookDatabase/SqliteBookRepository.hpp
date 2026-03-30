#pragma once

#include <filesystem>
#include <optional>

#include "Domain/BookRepository.hpp"

namespace LibriFlow::BookDatabase {

class CSqliteBookRepository final : public LibriFlow::Domain::IBookRepository
{
public:
    explicit CSqliteBookRepository(std::filesystem::path databasePath);

    [[nodiscard]] LibriFlow::Domain::SBookId Add(const LibriFlow::Domain::SBook& book) override;
    [[nodiscard]] std::optional<LibriFlow::Domain::SBook> GetById(LibriFlow::Domain::SBookId id) const override;
    void Remove(LibriFlow::Domain::SBookId id) override;

private:
    std::filesystem::path m_databasePath;
};

} // namespace LibriFlow::BookDatabase
