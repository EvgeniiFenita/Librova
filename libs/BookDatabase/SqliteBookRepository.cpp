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

#include "Domain/BookFormat.hpp"
#include "Domain/MetadataNormalization.hpp"
#include "SearchIndex/SearchIndexMaintenance.hpp"
#include "Sqlite/SqliteConnection.hpp"
#include "Sqlite/SqliteStatement.hpp"

namespace LibriFlow::BookDatabase {
namespace {

std::string PathToUtf8(const std::filesystem::path& path)
{
    const std::u8string utf8Path = path.generic_u8string();
    return std::string{reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size()};
}

std::filesystem::path Utf8ToPath(const std::string_view value)
{
    const auto* begin = reinterpret_cast<const char8_t*>(value.data());
    return std::filesystem::path(std::u8string{begin, begin + value.size()});
}

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
    explicit CSqliteTransaction(const LibriFlow::Sqlite::CSqliteConnection& connection)
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
    const LibriFlow::Sqlite::CSqliteConnection& m_connection;
    bool m_completed = false;
};

std::int64_t ResolveAuthorId(
    const LibriFlow::Sqlite::CSqliteConnection& connection,
    const std::string_view normalizedName,
    const std::string_view displayName)
{
    {
        LibriFlow::Sqlite::CSqliteStatement insertStatement(
            connection.GetNativeHandle(),
            "INSERT INTO authors (normalized_name, display_name) VALUES (?, ?) "
            "ON CONFLICT(normalized_name) DO NOTHING;");
        insertStatement.BindText(1, normalizedName);
        insertStatement.BindText(2, displayName);
        static_cast<void>(insertStatement.Step());
    }

    LibriFlow::Sqlite::CSqliteStatement selectStatement(
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
    const LibriFlow::Sqlite::CSqliteConnection& connection,
    const std::string_view normalizedName,
    const std::string_view displayName)
{
    {
        LibriFlow::Sqlite::CSqliteStatement insertStatement(
            connection.GetNativeHandle(),
            "INSERT INTO tags (normalized_name, display_name) VALUES (?, ?) "
            "ON CONFLICT(normalized_name) DO NOTHING;");
        insertStatement.BindText(1, normalizedName);
        insertStatement.BindText(2, displayName);
        static_cast<void>(insertStatement.Step());
    }

    LibriFlow::Sqlite::CSqliteStatement selectStatement(
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
    const LibriFlow::Sqlite::CSqliteConnection& connection,
    const std::int64_t bookId,
    const std::vector<std::string>& authors)
{
    std::unordered_set<std::string> insertedAuthors;

    for (std::size_t index = 0; index < authors.size(); ++index)
    {
        const std::string normalizedAuthor = LibriFlow::Domain::NormalizeText(authors[index]);

        if (normalizedAuthor.empty() || !insertedAuthors.insert(normalizedAuthor).second)
        {
            continue;
        }

        const std::int64_t authorId = ResolveAuthorId(connection, normalizedAuthor, authors[index]);

        LibriFlow::Sqlite::CSqliteStatement linkStatement(
            connection.GetNativeHandle(),
            "INSERT INTO book_authors (book_id, author_id, author_order) VALUES (?, ?, ?);");
        linkStatement.BindInt64(1, bookId);
        linkStatement.BindInt64(2, authorId);
        linkStatement.BindInt(3, static_cast<int>(index));
        static_cast<void>(linkStatement.Step());
    }
}

void InsertTags(
    const LibriFlow::Sqlite::CSqliteConnection& connection,
    const std::int64_t bookId,
    const std::vector<std::string>& tags)
{
    std::unordered_set<std::string> insertedTags;

    for (const std::string& tag : tags)
    {
        const std::string normalizedTag = LibriFlow::Domain::NormalizeText(tag);

        if (normalizedTag.empty() || !insertedTags.insert(normalizedTag).second)
        {
            continue;
        }

        const std::int64_t tagId = ResolveTagId(connection, normalizedTag, tag);

        LibriFlow::Sqlite::CSqliteStatement linkStatement(
            connection.GetNativeHandle(),
            "INSERT INTO book_tags (book_id, tag_id) VALUES (?, ?);");
        linkStatement.BindInt64(1, bookId);
        linkStatement.BindInt64(2, tagId);
        static_cast<void>(linkStatement.Step());
    }
}

void InsertFormatRow(
    const LibriFlow::Sqlite::CSqliteConnection& connection,
    const std::int64_t bookId,
    const LibriFlow::Domain::SBookFileInfo& fileInfo)
{
    LibriFlow::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "INSERT INTO formats (book_id, format, managed_path, file_size_bytes, sha256_hex) VALUES (?, ?, ?, ?, ?);");
    statement.BindInt64(1, bookId);
    statement.BindText(2, LibriFlow::Domain::ToString(fileInfo.Format));
    statement.BindText(3, PathToUtf8(fileInfo.ManagedPath));
    statement.BindInt64(4, static_cast<std::int64_t>(fileInfo.SizeBytes));
    statement.BindText(5, fileInfo.Sha256Hex);
    static_cast<void>(statement.Step());
}

std::vector<std::string> ReadAuthors(const LibriFlow::Sqlite::CSqliteConnection& connection, const std::int64_t bookId)
{
    LibriFlow::Sqlite::CSqliteStatement statement(
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

std::vector<std::string> ReadTags(const LibriFlow::Sqlite::CSqliteConnection& connection, const std::int64_t bookId)
{
    LibriFlow::Sqlite::CSqliteStatement statement(
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

std::optional<LibriFlow::Domain::SBookMetadata> ReadStoredMetadata(
    const LibriFlow::Sqlite::CSqliteConnection& connection,
    const LibriFlow::Domain::SBookId id)
{
    LibriFlow::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "SELECT title, language, series, series_index, publisher, year, isbn, description, identifier "
        "FROM books WHERE id = ?;");
    statement.BindInt64(1, id.Value);

    if (!statement.Step())
    {
        return std::nullopt;
    }

    LibriFlow::Domain::SBookMetadata metadata;
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

    return metadata;
}

} // namespace

CSqliteBookRepository::CSqliteBookRepository(std::filesystem::path databasePath)
    : m_databasePath(std::move(databasePath))
{
}

LibriFlow::Domain::SBookId CSqliteBookRepository::Add(const LibriFlow::Domain::SBook& book)
{
    LibriFlow::Sqlite::CSqliteConnection connection(m_databasePath);
    CSqliteTransaction transaction(connection);

    const std::optional<std::string> normalizedIsbn = LibriFlow::Domain::NormalizeIsbn(book.Metadata.Isbn);
    const std::string addedAtUtc = SerializeTimePoint(book.AddedAtUtc);
    const std::string managedPath = PathToUtf8(book.File.ManagedPath);
    const std::optional<std::string> coverPath = book.CoverPath.has_value()
        ? std::make_optional(PathToUtf8(*book.CoverPath))
        : std::nullopt;

    std::int64_t bookId = book.Id.Value;

    if (book.Id.IsValid())
    {
        LibriFlow::Sqlite::CSqliteStatement statement(
            connection.GetNativeHandle(),
            "INSERT INTO books "
            "(id, title, language, series, series_index, publisher, year, isbn, description, identifier, preferred_format, managed_path, cover_path, file_size_bytes, sha256_hex, added_at_utc) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
        statement.BindInt64(1, bookId);
        statement.BindText(2, book.Metadata.TitleUtf8);
        statement.BindText(3, book.Metadata.Language);
        book.Metadata.SeriesUtf8.has_value() ? statement.BindText(4, *book.Metadata.SeriesUtf8) : statement.BindNull(4);
        book.Metadata.SeriesIndex.has_value() ? statement.BindDouble(5, *book.Metadata.SeriesIndex) : statement.BindNull(5);
        book.Metadata.PublisherUtf8.has_value() ? statement.BindText(6, *book.Metadata.PublisherUtf8) : statement.BindNull(6);
        book.Metadata.Year.has_value() ? statement.BindInt(7, *book.Metadata.Year) : statement.BindNull(7);
        normalizedIsbn.has_value() ? statement.BindText(8, *normalizedIsbn) : statement.BindNull(8);
        book.Metadata.DescriptionUtf8.has_value() ? statement.BindText(9, *book.Metadata.DescriptionUtf8) : statement.BindNull(9);
        book.Metadata.Identifier.has_value() ? statement.BindText(10, *book.Metadata.Identifier) : statement.BindNull(10);
        statement.BindText(11, LibriFlow::Domain::ToString(book.File.Format));
        statement.BindText(12, managedPath);
        coverPath.has_value() ? statement.BindText(13, *coverPath) : statement.BindNull(13);
        statement.BindInt64(14, static_cast<std::int64_t>(book.File.SizeBytes));
        statement.BindText(15, book.File.Sha256Hex);
        statement.BindText(16, addedAtUtc);
        static_cast<void>(statement.Step());
    }
    else
    {
        LibriFlow::Sqlite::CSqliteStatement statement(
            connection.GetNativeHandle(),
            "INSERT INTO books "
            "(title, language, series, series_index, publisher, year, isbn, description, identifier, preferred_format, managed_path, cover_path, file_size_bytes, sha256_hex, added_at_utc) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");
        statement.BindText(1, book.Metadata.TitleUtf8);
        statement.BindText(2, book.Metadata.Language);
        book.Metadata.SeriesUtf8.has_value() ? statement.BindText(3, *book.Metadata.SeriesUtf8) : statement.BindNull(3);
        book.Metadata.SeriesIndex.has_value() ? statement.BindDouble(4, *book.Metadata.SeriesIndex) : statement.BindNull(4);
        book.Metadata.PublisherUtf8.has_value() ? statement.BindText(5, *book.Metadata.PublisherUtf8) : statement.BindNull(5);
        book.Metadata.Year.has_value() ? statement.BindInt(6, *book.Metadata.Year) : statement.BindNull(6);
        normalizedIsbn.has_value() ? statement.BindText(7, *normalizedIsbn) : statement.BindNull(7);
        book.Metadata.DescriptionUtf8.has_value() ? statement.BindText(8, *book.Metadata.DescriptionUtf8) : statement.BindNull(8);
        book.Metadata.Identifier.has_value() ? statement.BindText(9, *book.Metadata.Identifier) : statement.BindNull(9);
        statement.BindText(10, LibriFlow::Domain::ToString(book.File.Format));
        statement.BindText(11, managedPath);
        coverPath.has_value() ? statement.BindText(12, *coverPath) : statement.BindNull(12);
        statement.BindInt64(13, static_cast<std::int64_t>(book.File.SizeBytes));
        statement.BindText(14, book.File.Sha256Hex);
        statement.BindText(15, addedAtUtc);
        static_cast<void>(statement.Step());
        bookId = connection.GetLastInsertRowId();
    }

    InsertAuthors(connection, bookId, book.Metadata.AuthorsUtf8);
    InsertTags(connection, bookId, book.Metadata.TagsUtf8);
    InsertFormatRow(connection, bookId, book.File);
    LibriFlow::SearchIndex::CSearchIndexMaintenance::UpsertBook(connection, bookId, book.Metadata);

    transaction.Commit();
    return LibriFlow::Domain::SBookId{bookId};
}

LibriFlow::Domain::SBookId CSqliteBookRepository::ReserveId()
{
    LibriFlow::Sqlite::CSqliteConnection connection(m_databasePath);
    CSqliteTransaction transaction(connection);
    LibriFlow::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "UPDATE book_id_sequence "
        "SET next_id = next_id + 1 "
        "WHERE singleton = 1 "
        "RETURNING next_id - 1;");

    if (!statement.Step())
    {
        throw std::runtime_error("Failed to reserve next book id.");
    }

    const LibriFlow::Domain::SBookId reservedId{statement.GetColumnInt64(0)};
    transaction.Commit();
    return reservedId;
}

std::optional<LibriFlow::Domain::SBook> CSqliteBookRepository::GetById(const LibriFlow::Domain::SBookId id) const
{
    LibriFlow::Sqlite::CSqliteConnection connection(m_databasePath);
    LibriFlow::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "SELECT id, title, language, series, series_index, publisher, year, isbn, description, identifier, preferred_format, managed_path, cover_path, file_size_bytes, sha256_hex, added_at_utc "
        "FROM books WHERE id = ?;");
    statement.BindInt64(1, id.Value);

    if (!statement.Step())
    {
        return std::nullopt;
    }

    const std::optional<LibriFlow::Domain::EBookFormat> format = LibriFlow::Domain::TryParseBookFormat(statement.GetColumnText(10));

    if (!format.has_value())
    {
        throw std::runtime_error("Failed to parse stored book format.");
    }

    LibriFlow::Domain::SBook book;
    book.Id = LibriFlow::Domain::SBookId{statement.GetColumnInt64(0)};
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
    book.File.Format = *format;
    book.File.ManagedPath = Utf8ToPath(statement.GetColumnText(11));
    book.CoverPath = statement.IsColumnNull(12) ? std::nullopt : std::make_optional(Utf8ToPath(statement.GetColumnText(12)));
    book.File.SizeBytes = static_cast<std::uintmax_t>(statement.GetColumnInt64(13));
    book.File.Sha256Hex = statement.GetColumnText(14);
    book.AddedAtUtc = ParseTimePoint(statement.GetColumnText(15));

    return book;
}

void CSqliteBookRepository::Remove(const LibriFlow::Domain::SBookId id)
{
    LibriFlow::Sqlite::CSqliteConnection connection(m_databasePath);
    CSqliteTransaction transaction(connection);
    const std::optional<LibriFlow::Domain::SBookMetadata> existingMetadata = ReadStoredMetadata(connection, id);

    if (existingMetadata.has_value())
    {
        LibriFlow::SearchIndex::CSearchIndexMaintenance::RemoveBook(connection, id.Value, *existingMetadata);
    }

    LibriFlow::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), "DELETE FROM books WHERE id = ?;");
    statement.BindInt64(1, id.Value);
    static_cast<void>(statement.Step());
    transaction.Commit();
}

} // namespace LibriFlow::BookDatabase
