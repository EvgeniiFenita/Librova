#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <memory>
#include <span>
#include <vector>

#include "Domain/BookRepository.hpp"
#include "Sqlite/SqliteConnection.hpp"

namespace Librova::BookDatabase {

class CSqliteBookRepository final : public Librova::Domain::IBookRepository
{
public:
    explicit CSqliteBookRepository(std::filesystem::path databasePath);
    void CloseSession();

    [[nodiscard]] Librova::Domain::SBookId ReserveId() override;
    [[nodiscard]] std::vector<Librova::Domain::SBookId> ReserveIds(std::size_t count) override;
    [[nodiscard]] Librova::Domain::SBookId Add(const Librova::Domain::SBook& book) override;
    [[nodiscard]] Librova::Domain::SBookId ForceAdd(const Librova::Domain::SBook& book) override;
    [[nodiscard]] std::optional<Librova::Domain::SBook> GetById(Librova::Domain::SBookId id) const override;
    void Remove(Librova::Domain::SBookId id) override;
    void Compact() override;

    // Batch write: one transaction for all entries, per-item SAVEPOINT isolation.
    [[nodiscard]] std::vector<SBatchBookResult>
        AddBatch(std::span<const SBatchBookEntry> entries) override;

private:
    [[nodiscard]] Librova::Sqlite::CSqliteConnection& GetOrCreateConnection() const;

    std::filesystem::path m_databasePath;
    mutable std::mutex m_operationMutex;
    mutable std::unique_ptr<Librova::Sqlite::CSqliteConnection> m_connection;
};

} // namespace Librova::BookDatabase
