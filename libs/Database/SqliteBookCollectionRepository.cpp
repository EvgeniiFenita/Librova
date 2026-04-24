#include "Database/SqliteBookCollectionRepository.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sqlite3.h>

#include "Database/SqliteStatement.hpp"
#include "Database/SqliteTransaction.hpp"
#include "Database/SqliteEntityHelpers.hpp"
#include "Domain/DomainError.hpp"
#include "Domain/MetadataNormalization.hpp"

namespace Librova::BookDatabase {
namespace {

using Librova::Domain::CDomainException;
using Librova::Domain::EBookCollectionKind;
using Librova::Domain::EDomainErrorCode;
using Librova::Domain::SBookCollection;
using Librova::Domain::SBookId;

[[nodiscard]] std::string ToStorageValue(const EBookCollectionKind kind)
{
    return kind == EBookCollectionKind::Preset ? "preset" : "user";
}

[[nodiscard]] EBookCollectionKind ParseKind(const std::string& value)
{
    if (value == "user")
    {
        return EBookCollectionKind::User;
    }

    if (value == "preset")
    {
        return EBookCollectionKind::Preset;
    }

    throw std::runtime_error("Unknown stored collection kind.");
}

[[nodiscard]] SBookCollection ReadCollection(
    const Librova::Sqlite::CSqliteStatement& statement,
    const int columnOffset = 0)
{
    return {
        .Id = statement.GetColumnInt64(columnOffset),
        .NameUtf8 = statement.GetColumnText(columnOffset + 1),
        .IconKey = statement.GetColumnText(columnOffset + 2),
        .Kind = ParseKind(statement.GetColumnText(columnOffset + 3)),
        .IsDeletable = statement.GetColumnInt(columnOffset + 4) != 0
    };
}

void ValidateCreateRequest(const std::string& nameUtf8, const std::string& iconKey)
{
    if (Librova::Domain::NormalizeText(nameUtf8).empty())
    {
        throw CDomainException(EDomainErrorCode::Validation, "Collection name must not be empty.");
    }

    if (Librova::Domain::NormalizeText(iconKey).empty())
    {
        throw CDomainException(EDomainErrorCode::Validation, "Collection icon key must not be empty.");
    }
}

[[nodiscard]] bool HasBook(const Librova::Sqlite::CSqliteConnection& connection, const SBookId bookId)
{
    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "SELECT 1 FROM books WHERE id = ? LIMIT 1;");
    statement.BindInt64(1, bookId.Value);
    return statement.Step();
}

[[nodiscard]] std::optional<SBookCollection> ReadCollectionById(
    const Librova::Sqlite::CSqliteConnection& connection,
    const std::int64_t collectionId)
{
    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "SELECT id, name, icon_key, kind, is_deletable "
        "FROM collections "
        "WHERE id = ?;");
    statement.BindInt64(1, collectionId);

    if (!statement.Step())
    {
        return std::nullopt;
    }

    return ReadCollection(statement);
}

} // namespace

CSqliteBookCollectionRepository::CSqliteBookCollectionRepository(std::filesystem::path databasePath)
    : m_databasePath(std::move(databasePath))
{
}

Librova::Sqlite::CSqliteConnection& CSqliteBookCollectionRepository::GetOrCreateConnection() const
{
    if (m_connection == nullptr)
    {
        m_connection = std::make_unique<Librova::Sqlite::CSqliteConnection>(m_databasePath);
        m_connection->Execute("PRAGMA wal_autocheckpoint = 4000;");
    }

    return *m_connection;
}

void CSqliteBookCollectionRepository::CloseSession()
{
    const std::scoped_lock lock(m_operationMutex);
    m_connection.reset();
}

Librova::Domain::SBookCollection CSqliteBookCollectionRepository::CreateCollection(
    std::string nameUtf8,
    std::string iconKey,
    const Librova::Domain::EBookCollectionKind kind,
    const bool isDeletable)
{
    ValidateCreateRequest(nameUtf8, iconKey);

    const std::scoped_lock lock(m_operationMutex);
    auto& connection = GetOrCreateConnection();
    Librova::Sqlite::CSqliteTransaction transaction(connection);

    const std::string normalizedName = Librova::Domain::NormalizeText(nameUtf8);

    Librova::Sqlite::CSqliteStatement duplicateCheck(
        connection.GetNativeHandle(),
        "SELECT 1 FROM collections WHERE normalized_name = ? LIMIT 1;");
    duplicateCheck.BindText(1, normalizedName);
    if (duplicateCheck.Step())
    {
        throw CDomainException(
            EDomainErrorCode::Validation,
            "A collection with this name already exists.");
    }

    Librova::Sqlite::CSqliteStatement insertStatement(
        connection.GetNativeHandle(),
        "INSERT INTO collections (name, normalized_name, icon_key, kind, is_deletable, created_at_utc) "
        "VALUES (?, ?, ?, ?, ?, datetime('now'));");
    insertStatement.BindText(1, nameUtf8);
    insertStatement.BindText(2, normalizedName);
    insertStatement.BindText(3, iconKey);
    insertStatement.BindText(4, ToStorageValue(kind));
    insertStatement.BindInt(5, isDeletable ? 1 : 0);
    static_cast<void>(insertStatement.Step());

    const auto collection = ReadCollectionById(connection, connection.GetLastInsertRowId());
    if (!collection.has_value())
    {
        throw std::runtime_error("Failed to reload created collection.");
    }

    transaction.Commit();
    return *collection;
}

bool CSqliteBookCollectionRepository::DeleteCollection(const std::int64_t collectionId)
{
    const std::scoped_lock lock(m_operationMutex);
    auto& connection = GetOrCreateConnection();
    Librova::Sqlite::CSqliteTransaction transaction(connection);

    const auto collection = ReadCollectionById(connection, collectionId);
    if (!collection.has_value())
    {
        return false;
    }

    if (!collection->IsDeletable)
    {
        throw CDomainException(
            EDomainErrorCode::Validation,
            "This collection cannot be deleted.");
    }

    Librova::Sqlite::CSqliteStatement deleteStatement(
        connection.GetNativeHandle(),
        "DELETE FROM collections WHERE id = ?;");
    deleteStatement.BindInt64(1, collectionId);
    static_cast<void>(deleteStatement.Step());

    transaction.Commit();
    return sqlite3_changes(connection.GetNativeHandle()) > 0;
}

bool CSqliteBookCollectionRepository::AddBookToCollection(const SBookId bookId, const std::int64_t collectionId)
{
    if (!bookId.IsValid())
    {
        throw CDomainException(EDomainErrorCode::Validation, "Book id must be valid.");
    }

    const std::scoped_lock lock(m_operationMutex);
    auto& connection = GetOrCreateConnection();
    Librova::Sqlite::CSqliteTransaction transaction(connection);

    if (!HasBook(connection, bookId))
    {
        throw CDomainException(EDomainErrorCode::NotFound, "Book was not found.");
    }

    if (!ReadCollectionById(connection, collectionId).has_value())
    {
        throw CDomainException(EDomainErrorCode::NotFound, "Collection was not found.");
    }

    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "INSERT OR IGNORE INTO book_collections (book_id, collection_id) VALUES (?, ?);");
    statement.BindInt64(1, bookId.Value);
    statement.BindInt64(2, collectionId);
    static_cast<void>(statement.Step());
    transaction.Commit();
    return sqlite3_changes(connection.GetNativeHandle()) > 0;
}

bool CSqliteBookCollectionRepository::RemoveBookFromCollection(const SBookId bookId, const std::int64_t collectionId)
{
    if (!bookId.IsValid())
    {
        throw CDomainException(EDomainErrorCode::Validation, "Book id must be valid.");
    }

    const std::scoped_lock lock(m_operationMutex);
    auto& connection = GetOrCreateConnection();
    Librova::Sqlite::CSqliteTransaction transaction(connection);

    if (!ReadCollectionById(connection, collectionId).has_value())
    {
        throw CDomainException(EDomainErrorCode::NotFound, "Collection was not found.");
    }

    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "DELETE FROM book_collections WHERE book_id = ? AND collection_id = ?;");
    statement.BindInt64(1, bookId.Value);
    statement.BindInt64(2, collectionId);
    static_cast<void>(statement.Step());
    transaction.Commit();
    return sqlite3_changes(connection.GetNativeHandle()) > 0;
}

std::vector<SBookCollection> CSqliteBookCollectionRepository::ListCollections() const
{
    Librova::Sqlite::CSqliteConnection connection(m_databasePath);
    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "SELECT id, name, icon_key, kind, is_deletable "
        "FROM collections "
        "ORDER BY normalized_name ASC, id ASC;");

    std::vector<SBookCollection> collections;
    while (statement.Step())
    {
        collections.push_back(ReadCollection(statement));
    }

    return collections;
}

std::vector<SBookCollection> CSqliteBookCollectionRepository::ListCollectionsForBook(const SBookId bookId) const
{
    if (!bookId.IsValid())
    {
        return {};
    }

    Librova::Sqlite::CSqliteConnection connection(m_databasePath);
    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "SELECT c.id, c.name, c.icon_key, c.kind, c.is_deletable "
        "FROM book_collections bc "
        "INNER JOIN collections c ON c.id = bc.collection_id "
        "WHERE bc.book_id = ? "
        "ORDER BY c.normalized_name ASC, c.id ASC;");
    statement.BindInt64(1, bookId.Value);

    std::vector<SBookCollection> collections;
    while (statement.Step())
    {
        collections.push_back(ReadCollection(statement));
    }

    return collections;
}

std::unordered_map<std::int64_t, std::vector<SBookCollection>> CSqliteBookCollectionRepository::ListCollectionsForBooks(
    const std::vector<SBookId>& bookIds) const
{
    std::vector<std::int64_t> validBookIds;
    validBookIds.reserve(bookIds.size());
    for (const auto bookId : bookIds)
    {
        if (bookId.IsValid())
        {
            validBookIds.push_back(bookId.Value);
        }
    }

    if (validBookIds.empty())
    {
        return {};
    }

    Librova::Sqlite::CSqliteConnection connection(m_databasePath);
    const std::string sql =
        "SELECT bc.book_id, c.id, c.name, c.icon_key, c.kind, c.is_deletable "
        "FROM book_collections bc "
        "INNER JOIN collections c ON c.id = bc.collection_id "
        "WHERE bc.book_id IN " + Librova::Sqlite::BuildIdInClause(validBookIds.size()) + " "
        "ORDER BY bc.book_id ASC, c.normalized_name ASC, c.id ASC;";

    Librova::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), sql);
    int parameterIndex = 1;
    for (const auto bookId : validBookIds)
    {
        statement.BindInt64(parameterIndex++, bookId);
    }

    std::unordered_map<std::int64_t, std::vector<SBookCollection>> collectionsByBookId;
    while (statement.Step())
    {
        collectionsByBookId[statement.GetColumnInt64(0)].push_back(ReadCollection(statement, 1));
    }

    return collectionsByBookId;
}

std::optional<SBookCollection> CSqliteBookCollectionRepository::GetCollectionById(const std::int64_t collectionId) const
{
    Librova::Sqlite::CSqliteConnection connection(m_databasePath);
    return ReadCollectionById(connection, collectionId);
}

} // namespace Librova::BookDatabase
