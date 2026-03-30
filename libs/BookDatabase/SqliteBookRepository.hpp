#pragma once

#include <filesystem>
#include <optional>

#include "Domain/BookRepository.hpp"

namespace Librova::BookDatabase {

class CSqliteBookRepository final : public Librova::Domain::IBookRepository
{
public:
    explicit CSqliteBookRepository(std::filesystem::path databasePath);

    [[nodiscard]] Librova::Domain::SBookId ReserveId() override;
    [[nodiscard]] Librova::Domain::SBookId Add(const Librova::Domain::SBook& book) override;
    [[nodiscard]] std::optional<Librova::Domain::SBook> GetById(Librova::Domain::SBookId id) const override;
    void Remove(Librova::Domain::SBookId id) override;

private:
    std::filesystem::path m_databasePath;
};

} // namespace Librova::BookDatabase
