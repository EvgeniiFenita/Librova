#include "BookDatabase/SqliteBookRepository.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "BookDatabase/SqliteGenreHelpers.hpp"
#include "Domain/BookFormat.hpp"
#include "Domain/MetadataNormalization.hpp"
#include "Domain/StorageEncoding.hpp"
#include "SearchIndex/SearchIndexMaintenance.hpp"
#include "Sqlite/SqliteConnection.hpp"
#include "Sqlite/SqliteStatement.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace Librova::BookDatabase {
namespace {

std::string SerializeTimePoint(const std::chrono::system_clock::time_point value)
{
    return std::format("{:%Y-%m-%dT%H:%M:%SZ}", std::chrono::floor<std::chrono::seconds>(value));
}

std::chrono::system_clock::time_point ParseTimePoint(const std::string_view value)
{
    std::istringstream input{std::string{value}};
    std::chrono::sys_seconds parsedValue{};
    input >> std::chrono::parse("%Y-%m-%dT%H:%M:%SZ", parsedValue);

    if (input.fail())
    {
        throw std::runtime_error(std::string{"Failed to parse sqlite timestamp: "} + std::string{value});
    }

    return parsedValue;
}

class CSqliteTransaction final
{
public:
    explicit CSqliteTransaction(const Librova::Sqlite::CSqliteConnection& connection)
        : m_connection(connection)
    {
        m_connection.Execute("BEGIN IMMEDIATE;");
    }

    ~CSqliteTransaction()
    {
        if (!m_completed)
        {
            try
            {
                m_connection.Execute("ROLLBACK;");
            }
            catch (...)
            {
            }
        }
    }

    void Commit()
    {
        m_connection.Execute("COMMIT;");
        m_completed = true;
    }

private:
    const Librova::Sqlite::CSqliteConnection& m_connection;
    bool m_completed = false;
};

std::int64_t ResolveAuthorId(
    const Librova::Sqlite::CSqliteConnection& connection,
    const std::string_view normalizedName,
    const std::string_view displayName)
{
    {
        Librova::Sqlite::CSqliteStatement insertStatement(
            connection.GetNativeHandle(),
            "INSERT INTO authors (normalized_name, display_name) VALUES (?, ?) "
            "ON CONFLICT(normalized_name) DO NOTHING;");
        insertStatement.BindText(1, normalizedName);
        insertStatement.BindText(2, displayName);
        static_cast<void>(insertStatement.Step());
    }

    Librova::Sqlite::CSqliteStatement selectStatement(
        connection.GetNativeHandle(),
        "SELECT id FROM authors WHERE normalized_name = ?;");
    selectStatement.BindText(1, normalizedName);

    if (!selectStatement.Step())
    {
        throw std::runtime_error("Failed to resolve author id after insert.");
    }

    return selectStatement.GetColumnInt64(0);
}

std::int64_t ResolveTagId(
    const Librova::Sqlite::CSqliteConnection& connection,
    const std::string_view normalizedName,
    const std::string_view displayName)
{
    {
        Librova::Sqlite::CSqliteStatement insertStatement(
            connection.GetNativeHandle(),
            "INSERT INTO tags (normalized_name, display_name) VALUES (?, ?) "
            "ON CONFLICT(normalized_name) DO NOTHING;");
        insertStatement.BindText(1, normalizedName);
        insertStatement.BindText(2, displayName);
        static_cast<void>(insertStatement.Step());
    }

    Librova::Sqlite::CSqliteStatement selectStatement(
        connection.GetNativeHandle(),
        "SELECT id FROM tags WHERE normalized_name = ?;");
    selectStatement.BindText(1, normalizedName);

    if (!selectStatement.Step())
    {
        throw std::runtime_error("Failed to resolve tag id after insert.");
    }

    return selectStatement.GetColumnInt64(0);
}

void InsertAuthors(
    const Librova::Sqlite::CSqliteConnection& connection,
    const std::int64_t bookId,
    const std::vector<std::string>& authors)
{
    std::unordered_set<std::string> insertedAuthors;

    for (std::size_t index = 0; index < authors.size(); ++index)
    {
        const std::string normalizedAuthor = Librova::Domain::NormalizeText(authors[index]);

        if (normalizedAuthor.empty() || !insertedAuthors.insert(normalizedAuthor).second)
        {
            continue;
        }

        const std::int64_t authorId = ResolveAuthorId(connection, normalizedAuthor, authors[index]);

        Librova::Sqlite::CSqliteStatement linkStatement(
            connection.GetNativeHandle(),
            "INSERT INTO book_authors (book_id, author_id, author_order) VALUES (?, ?, ?);");
        linkStatement.BindInt64(1, bookId);
        linkStatement.BindInt64(2, authorId);
        linkStatement.BindInt(3, static_cast<int>(index));
        static_cast<void>(linkStatement.Step());
    }
}

void InsertTags(
    const Librova::Sqlite::CSqliteConnection& connection,
    const std::int64_t bookId,
    const std::vector<std::string>& tags)
{
    std::unordered_set<std::string> insertedTags;

    for (const std::string& tag : tags)
    {
        const std::string normalizedTag = Librova::Domain::NormalizeText(tag);

        if (normalizedTag.empty() || !insertedTags.insert(normalizedTag).second)
        {
            continue;
        }

        const std::int64_t tagId = ResolveTagId(connection, normalizedTag, tag);

        Librova::Sqlite::CSqliteStatement linkStatement(
            connection.GetNativeHandle(),
            "INSERT INTO book_tags (book_id, tag_id) VALUES (?, ?);");
        linkStatement.BindInt64(1, bookId);
        linkStatement.BindInt64(2, tagId);
        static_cast<void>(linkStatement.Step());
    }
}

std::vector<std::string> ReadAuthors(const Librova::Sqlite::CSqliteConnection& connection, const std::int64_t bookId)
{
    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "SELECT a.display_name "
        "FROM book_authors ba "
        "INNER JOIN authors a ON a.id = ba.author_id "
        "WHERE ba.book_id = ? "
        "ORDER BY ba.author_order ASC;");
    statement.BindInt64(1, bookId);

    std::vector<std::string> authors;

    while (statement.Step())
    {
        authors.push_back(statement.GetColumnText(0));
    }

    return authors;
}

std::vector<std::string> ReadTags(const Librova::Sqlite::CSqliteConnection& connection, const std::int64_t bookId)
{
    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "SELECT t.display_name "
        "FROM book_tags bt "
        "INNER JOIN tags t ON t.id = bt.tag_id "
        "WHERE bt.book_id = ? "
        "ORDER BY t.display_name ASC;");
    statement.BindInt64(1, bookId);

    std::vector<std::string> tags;

    while (statement.Step())
    {
        tags.push_back(statement.GetColumnText(0));
    }

    return tags;
}

using Librova::BookDatabase::CSqliteGenreHelpers;

std::vector<std::string> ReadGenres(const Librova::Sqlite::CSqliteConnection& connection, const std::int64_t bookId)
{
    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "SELECT g.display_name "
        "FROM book_genres bg "
        "INNER JOIN genres g ON g.id = bg.genre_id "
        "WHERE bg.book_id = ? "
        "ORDER BY g.display_name ASC;");
    statement.BindInt64(1, bookId);

    std::vector<std::string> genres;

    while (statement.Step())
    {
        genres.push_back(statement.GetColumnText(0));
    }

    return genres;
}

std::optional<Librova::Domain::SBookMetadata> ReadStoredMetadata(
    const Librova::Sqlite::CSqliteConnection& connection,
    const Librova::Domain::SBookId id)
{
    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "SELECT title, language, series, series_index, publisher, year, isbn, description, identifier "
        "FROM books WHERE id = ?;");
    statement.BindInt64(1, id.Value);

    if (!statement.Step())
    {
        return std::nullopt;
    }

    Librova::Domain::SBookMetadata metadata;
    metadata.TitleUtf8 = statement.GetColumnText(0);
    metadata.Language = statement.GetColumnText(1);
    metadata.SeriesUtf8 = statement.IsColumnNull(2) ? std::nullopt : std::make_optional(statement.GetColumnText(2));
    metadata.SeriesIndex = statement.IsColumnNull(3) ? std::nullopt : std::make_optional(statement.GetColumnDouble(3));
    metadata.PublisherUtf8 = statement.IsColumnNull(4) ? std::nullopt : std::make_optional(statement.GetColumnText(4));
    metadata.Year = statement.IsColumnNull(5) ? std::nullopt : std::make_optional(statement.GetColumnInt(5));
    metadata.Isbn = statement.IsColumnNull(6) ? std::nullopt : std::make_optional(statement.GetColumnText(6));
    metadata.DescriptionUtf8 = statement.IsColumnNull(7) ? std::nullopt : std::make_optional(statement.GetColumnText(7));
    metadata.Identifier = statement.IsColumnNull(8) ? std::nullopt : std::make_optional(statement.GetColumnText(8));
    metadata.AuthorsUtf8 = ReadAuthors(connection, id.Value);
    metadata.TagsUtf8 = ReadTags(connection, id.Value);
    metadata.GenresUtf8 = ReadGenres(connection, id.Value);

    return metadata;
}

std::int64_t DoAddBook(
    const Librova::Sqlite::CSqliteConnection& connection,
    const Librova::Domain::SBook& book)
{
    const std::optional<std::string> normalizedIsbn = Librova::Domain::NormalizeIsbn(book.Metadata.Isbn);
    const std::string normalizedTitle = Librova::Domain::NormalizeText(book.Metadata.TitleUtf8);
    const std::string addedAtUtc = SerializeTimePoint(book.AddedAtUtc);
    const std::string managedPath = Librova::Unicode::PathToUtf8(book.File.ManagedPath);
    const std::optional<std::string> coverPath = book.CoverPath.has_value()
        ? std::make_optional(Librova::Unicode::PathToUtf8(*book.CoverPath))
        : std::nullopt;

    std::int64_t bookId = book.Id.Value;

    if (book.Id.IsValid())
    {
        Librova::Sqlite::CSqliteStatement statement(
            connection.GetNativeHandle(),
            "INSERT INTO books "
            "(id, title, normalized_title, language, series, series_index, publisher, year, isbn, description, identifier, preferred_format, storage_encoding, managed_path, cover_path, file_size_bytes, sha256_hex, added_at_utc) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
        statement.BindInt64(1, bookId);
        statement.BindText(2, book.Metadata.TitleUtf8);
        statement.BindText(3, normalizedTitle);
        statement.BindText(4, book.Metadata.Language);
        book.Metadata.SeriesUtf8.has_value() ? statement.BindText(5, *book.Metadata.SeriesUtf8) : statement.BindNull(5);
        book.Metadata.SeriesIndex.has_value() ? statement.BindDouble(6, *book.Metadata.SeriesIndex) : statement.BindNull(6);
        book.Metadata.PublisherUtf8.has_value() ? statement.BindText(7, *book.Metadata.PublisherUtf8) : statement.BindNull(7);
        book.Metadata.Year.has_value() ? statement.BindInt(8, *book.Metadata.Year) : statement.BindNull(8);
        normalizedIsbn.has_value() ? statement.BindText(9, *normalizedIsbn) : statement.BindNull(9);
        book.Metadata.DescriptionUtf8.has_value() ? statement.BindText(10, *book.Metadata.DescriptionUtf8) : statement.BindNull(10);
        book.Metadata.Identifier.has_value() ? statement.BindText(11, *book.Metadata.Identifier) : statement.BindNull(11);
        statement.BindText(12, Librova::Domain::ToString(book.File.Format));
        statement.BindText(13, Librova::Domain::ToString(book.File.StorageEncoding));
        statement.BindText(14, managedPath);
        coverPath.has_value() ? statement.BindText(15, *coverPath) : statement.BindNull(15);
        statement.BindInt64(16, static_cast<std::int64_t>(book.File.SizeBytes));
        statement.BindText(17, book.File.Sha256Hex);
        statement.BindText(18, addedAtUtc);
        static_cast<void>(statement.Step());
    }
    else
    {
        Librova::Sqlite::CSqliteStatement statement(
            connection.GetNativeHandle(),
            "INSERT INTO books "
            "(title, normalized_title, language, series, series_index, publisher, year, isbn, description, identifier, preferred_format, storage_encoding, managed_path, cover_path, file_size_bytes, sha256_hex, added_at_utc) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
        statement.BindText(1, book.Metadata.TitleUtf8);
        statement.BindText(2, normalizedTitle);
        statement.BindText(3, book.Metadata.Language);
        book.Metadata.SeriesUtf8.has_value() ? statement.BindText(4, *book.Metadata.SeriesUtf8) : statement.BindNull(4);
        book.Metadata.SeriesIndex.has_value() ? statement.BindDouble(5, *book.Metadata.SeriesIndex) : statement.BindNull(5);
        book.Metadata.PublisherUtf8.has_value() ? statement.BindText(6, *book.Metadata.PublisherUtf8) : statement.BindNull(6);
        book.Metadata.Year.has_value() ? statement.BindInt(7, *book.Metadata.Year) : statement.BindNull(7);
        normalizedIsbn.has_value() ? statement.BindText(8, *normalizedIsbn) : statement.BindNull(8);
        book.Metadata.DescriptionUtf8.has_value() ? statement.BindText(9, *book.Metadata.DescriptionUtf8) : statement.BindNull(9);
        book.Metadata.Identifier.has_value() ? statement.BindText(10, *book.Metadata.Identifier) : statement.BindNull(10);
        statement.BindText(11, Librova::Domain::ToString(book.File.Format));
        statement.BindText(12, Librova::Domain::ToString(book.File.StorageEncoding));
        statement.BindText(13, managedPath);
        coverPath.has_value() ? statement.BindText(14, *coverPath) : statement.BindNull(14);
        statement.BindInt64(15, static_cast<std::int64_t>(book.File.SizeBytes));
        statement.BindText(16, book.File.Sha256Hex);
        statement.BindText(17, addedAtUtc);
        static_cast<void>(statement.Step());
        bookId = connection.GetLastInsertRowId();
    }

    InsertAuthors(connection, bookId, book.Metadata.AuthorsUtf8);
    InsertTags(connection, bookId, book.Metadata.TagsUtf8);

    const std::string_view genreSourceType =
        book.File.Format == Librova::Domain::EBookFormat::Fb2 ? "fb2_genre" : "epub_subject";
    CSqliteGenreHelpers::InsertGenres(connection, bookId, book.Metadata.GenresUtf8, genreSourceType);
    Librova::SearchIndex::CSearchIndexMaintenance::InsertBook(connection, bookId, book.Metadata);

    return bookId;
}

} // namespace

CSqliteBookRepository::CSqliteBookRepository(std::filesystem::path databasePath)
    : m_databasePath(std::move(databasePath))
{
}

void CSqliteBookRepository::CloseSession()
{
    const std::scoped_lock lock(m_operationMutex);
    m_connection.reset();
}

Librova::Sqlite::CSqliteConnection& CSqliteBookRepository::GetOrCreateConnection() const
{
    if (m_connection == nullptr)
    {
        m_connection = std::make_unique<Librova::Sqlite::CSqliteConnection>(m_databasePath);
    }

    return *m_connection;
}

Librova::Domain::SBookId CSqliteBookRepository::Add(const Librova::Domain::SBook& book)
{
    const std::scoped_lock lock(m_operationMutex);
    auto& connection = GetOrCreateConnection();
    CSqliteTransaction transaction(connection);

    if (!book.File.Sha256Hex.empty())
    {
        Librova::Sqlite::CSqliteStatement hashCheck(
            connection.GetNativeHandle(),
            "SELECT id FROM books WHERE sha256_hex = ? LIMIT 1;");
        hashCheck.BindText(1, book.File.Sha256Hex);

        if (hashCheck.Step())
        {
            throw Librova::Domain::CDuplicateHashException{
                Librova::Domain::SBookId{hashCheck.GetColumnInt64(0)}};
        }
    }

    const Librova::Domain::SBookId bookId{DoAddBook(connection, book)};
    transaction.Commit();
    return bookId;
}

Librova::Domain::SBookId CSqliteBookRepository::ForceAdd(const Librova::Domain::SBook& book)
{
    const std::scoped_lock lock(m_operationMutex);
    auto& connection = GetOrCreateConnection();
    CSqliteTransaction transaction(connection);
    const Librova::Domain::SBookId bookId{DoAddBook(connection, book)};
    transaction.Commit();
    return bookId;
}

Librova::Domain::SBookId CSqliteBookRepository::ReserveId()
{
    const std::scoped_lock lock(m_operationMutex);
    auto& connection = GetOrCreateConnection();
    CSqliteTransaction transaction(connection);
    Librova::Sqlite::CSqliteStatement selectStatement(
        connection.GetNativeHandle(),
        "SELECT next_id FROM book_id_sequence WHERE singleton = 1;");

    if (!selectStatement.Step())
    {
        throw std::runtime_error("Failed to read next reserved book id.");
    }

    const std::int64_t nextId = selectStatement.GetColumnInt64(0);

    Librova::Sqlite::CSqliteStatement updateStatement(
        connection.GetNativeHandle(),
        "UPDATE book_id_sequence SET next_id = ? WHERE singleton = 1;");
    updateStatement.BindInt64(1, nextId + 1);
    static_cast<void>(updateStatement.Step());

    transaction.Commit();
    return Librova::Domain::SBookId{nextId};
}

std::optional<Librova::Domain::SBook> CSqliteBookRepository::GetById(const Librova::Domain::SBookId id) const
{
    const std::scoped_lock lock(m_operationMutex);
    auto& connection = GetOrCreateConnection();
    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "SELECT id, title, language, series, series_index, publisher, year, isbn, description, identifier, preferred_format, storage_encoding, managed_path, cover_path, file_size_bytes, sha256_hex, added_at_utc "
        "FROM books WHERE id = ?;");
    statement.BindInt64(1, id.Value);

    if (!statement.Step())
    {
        return std::nullopt;
    }

    const std::optional<Librova::Domain::EBookFormat> format = Librova::Domain::TryParseBookFormat(statement.GetColumnText(10));
    const std::optional<Librova::Domain::EStorageEncoding> storageEncoding = Librova::Domain::TryParseStorageEncoding(statement.GetColumnText(11));

    if (!format.has_value() || !storageEncoding.has_value())
    {
        throw std::runtime_error("Failed to parse stored book file metadata.");
    }

    Librova::Domain::SBook book;
    book.Id = Librova::Domain::SBookId{statement.GetColumnInt64(0)};
    book.Metadata.TitleUtf8 = statement.GetColumnText(1);
    book.Metadata.Language = statement.GetColumnText(2);
    book.Metadata.SeriesUtf8 = statement.IsColumnNull(3) ? std::nullopt : std::make_optional(statement.GetColumnText(3));
    book.Metadata.SeriesIndex = statement.IsColumnNull(4) ? std::nullopt : std::make_optional(statement.GetColumnDouble(4));
    book.Metadata.PublisherUtf8 = statement.IsColumnNull(5) ? std::nullopt : std::make_optional(statement.GetColumnText(5));
    book.Metadata.Year = statement.IsColumnNull(6) ? std::nullopt : std::make_optional(statement.GetColumnInt(6));
    book.Metadata.Isbn = statement.IsColumnNull(7) ? std::nullopt : std::make_optional(statement.GetColumnText(7));
    book.Metadata.DescriptionUtf8 = statement.IsColumnNull(8) ? std::nullopt : std::make_optional(statement.GetColumnText(8));
    book.Metadata.Identifier = statement.IsColumnNull(9) ? std::nullopt : std::make_optional(statement.GetColumnText(9));
    book.Metadata.AuthorsUtf8 = ReadAuthors(connection, book.Id.Value);
    book.Metadata.TagsUtf8 = ReadTags(connection, book.Id.Value);
    book.Metadata.GenresUtf8 = ReadGenres(connection, book.Id.Value);
    book.File.Format = *format;
    book.File.StorageEncoding = *storageEncoding;
    book.File.ManagedPath = Librova::Unicode::PathFromUtf8(statement.GetColumnText(12));
    book.CoverPath = statement.IsColumnNull(13) ? std::nullopt : std::make_optional(Librova::Unicode::PathFromUtf8(statement.GetColumnText(13)));
    book.File.SizeBytes = static_cast<std::uintmax_t>(statement.GetColumnInt64(14));
    book.File.Sha256Hex = statement.GetColumnText(15);
    book.AddedAtUtc = ParseTimePoint(statement.GetColumnText(16));

    return book;
}

void CSqliteBookRepository::Remove(const Librova::Domain::SBookId id)
{
    const std::scoped_lock lock(m_operationMutex);
    auto& connection = GetOrCreateConnection();
    CSqliteTransaction transaction(connection);
    const std::optional<Librova::Domain::SBookMetadata> existingMetadata = ReadStoredMetadata(connection, id);

    if (existingMetadata.has_value())
    {
        Librova::SearchIndex::CSearchIndexMaintenance::RemoveBook(connection, id.Value, *existingMetadata);
    }

    Librova::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), "DELETE FROM books WHERE id = ?;");
    statement.BindInt64(1, id.Value);
    static_cast<void>(statement.Step());
    transaction.Commit();
}

void CSqliteBookRepository::Compact()
{
    const std::scoped_lock lock(m_operationMutex);
    auto& connection = GetOrCreateConnection();
    connection.Execute("VACUUM;");
}

} // namespace Librova::BookDatabase
