#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "Domain/BookCollection.hpp"
#include "Database/SqliteConnection.hpp"

namespace Librova::BookDatabase {

class CSqliteBookCollectionRepository final
    : public Librova::Domain::IBookCollectionRepository
    , public Librova::Domain::IBookCollectionQueryRepository
{
public:
    explicit CSqliteBookCollectionRepository(std::filesystem::path databasePath);

    [[nodiscard]] Librova::Domain::SBookCollection CreateCollection(
        std::string nameUtf8,
        std::string iconKey,
        Librova::Domain::EBookCollectionKind kind = Librova::Domain::EBookCollectionKind::User,
        bool isDeletable = true) override;

    [[nodiscard]] bool DeleteCollection(std::int64_t collectionId) override;
    [[nodiscard]] bool AddBookToCollection(Librova::Domain::SBookId bookId, std::int64_t collectionId) override;
    [[nodiscard]] bool RemoveBookFromCollection(Librova::Domain::SBookId bookId, std::int64_t collectionId) override;

    [[nodiscard]] std::vector<Librova::Domain::SBookCollection> ListCollections() const override;
    [[nodiscard]] std::vector<Librova::Domain::SBookCollection> ListCollectionsForBook(Librova::Domain::SBookId bookId) const override;
    [[nodiscard]] std::unordered_map<std::int64_t, std::vector<Librova::Domain::SBookCollection>> ListCollectionsForBooks(
        const std::vector<Librova::Domain::SBookId>& bookIds) const override;
    [[nodiscard]] std::optional<Librova::Domain::SBookCollection> GetCollectionById(std::int64_t collectionId) const override;

    void CloseSession();

private:
    [[nodiscard]] Librova::Sqlite::CSqliteConnection& GetOrCreateConnection() const;

    std::filesystem::path m_databasePath;
    mutable std::mutex m_operationMutex;
    mutable std::unique_ptr<Librova::Sqlite::CSqliteConnection> m_connection;
};

} // namespace Librova::BookDatabase
