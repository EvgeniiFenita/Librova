#include "Database/SearchIndexMaintenance.hpp"

#include <string>
#include <vector>

#include "Domain/MetadataNormalization.hpp"
#include "Database/SqliteStatement.hpp"

namespace Librova::SearchIndex {
namespace {

std::string JoinNormalizedText(const std::vector<std::string>& values)
{
    std::string joined;

    for (const std::string& value : values)
    {
        const std::string normalized = Librova::Domain::NormalizeText(value);

        if (normalized.empty())
        {
            continue;
        }

        if (!joined.empty())
        {
            joined.push_back(' ');
        }

        joined.append(normalized);
    }

    return joined;
}

std::string BuildNormalizedDescription(const std::optional<std::string>& description)
{
    if (!description.has_value())
    {
        return {};
    }

    return Librova::Domain::NormalizeText(*description);
}

[[nodiscard]] bool HasTable(
    const Librova::Sqlite::CSqliteConnection& connection,
    const std::string_view tableName)
{
    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = ?;");
    statement.BindText(1, tableName);
    return statement.Step() && statement.GetColumnInt(0) > 0;
}

void ExecuteDeleteCommand(
    const Librova::Sqlite::CSqliteConnection& connection,
    const std::int64_t bookId,
    const Librova::Domain::SBookMetadata& metadata)
{
    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "INSERT INTO search_index(search_index, rowid, title, authors, tags, genres, description) VALUES('delete', ?, ?, ?, ?, ?, ?);");
    statement.BindInt64(1, bookId);
    statement.BindText(2, Librova::Domain::NormalizeText(metadata.TitleUtf8));
    statement.BindText(3, JoinNormalizedText(metadata.AuthorsUtf8));
    statement.BindText(4, JoinNormalizedText(metadata.TagsUtf8));
    statement.BindText(5, JoinNormalizedText(metadata.GenresUtf8));
    statement.BindText(6, BuildNormalizedDescription(metadata.DescriptionUtf8));
    static_cast<void>(statement.Step());
}

struct SBookSearchMetadata
{
    std::int64_t BookId;
    Librova::Domain::SBookMetadata Metadata;
};

[[nodiscard]] std::vector<SBookSearchMetadata> ReadAllBooksForRebuild(
    const Librova::Sqlite::CSqliteConnection& connection)
{
    std::vector<SBookSearchMetadata> books;

    {
        Librova::Sqlite::CSqliteStatement statement(
            connection.GetNativeHandle(),
            "SELECT id, title, description FROM books ORDER BY id ASC;");

        while (statement.Step())
        {
            Librova::Domain::SBookMetadata metadata;
            metadata.TitleUtf8 = statement.GetColumnText(1);
            metadata.DescriptionUtf8 = statement.IsColumnNull(2)
                ? std::nullopt
                : std::make_optional(statement.GetColumnText(2));

            books.push_back({
                .BookId = statement.GetColumnInt64(0),
                .Metadata = std::move(metadata)
            });
        }
    }

    const bool hasAuthors = HasTable(connection, "book_authors") && HasTable(connection, "authors");
    const bool hasTags = HasTable(connection, "book_tags") && HasTable(connection, "tags");
    const bool hasGenres = HasTable(connection, "book_genres") && HasTable(connection, "genres");

    for (SBookSearchMetadata& book : books)
    {
        if (hasAuthors)
        {
            Librova::Sqlite::CSqliteStatement authorsStatement(
                connection.GetNativeHandle(),
                "SELECT a.display_name "
                "FROM book_authors ba "
                "INNER JOIN authors a ON a.id = ba.author_id "
                "WHERE ba.book_id = ? "
                "ORDER BY ba.author_order ASC;");
            authorsStatement.BindInt64(1, book.BookId);

            while (authorsStatement.Step())
            {
                book.Metadata.AuthorsUtf8.push_back(authorsStatement.GetColumnText(0));
            }
        }

        if (hasTags)
        {
            Librova::Sqlite::CSqliteStatement tagsStatement(
                connection.GetNativeHandle(),
                "SELECT t.display_name "
                "FROM book_tags bt "
                "INNER JOIN tags t ON t.id = bt.tag_id "
                "WHERE bt.book_id = ? "
                "ORDER BY t.display_name ASC;");
            tagsStatement.BindInt64(1, book.BookId);

            while (tagsStatement.Step())
            {
                book.Metadata.TagsUtf8.push_back(tagsStatement.GetColumnText(0));
            }
        }

        if (hasGenres)
        {
            Librova::Sqlite::CSqliteStatement genresStatement(
                connection.GetNativeHandle(),
                "SELECT g.display_name "
                "FROM book_genres bg "
                "INNER JOIN genres g ON g.id = bg.genre_id "
                "WHERE bg.book_id = ? "
                "ORDER BY g.display_name ASC;");
            genresStatement.BindInt64(1, book.BookId);

            while (genresStatement.Step())
            {
                book.Metadata.GenresUtf8.push_back(genresStatement.GetColumnText(0));
            }
        }
    }

    return books;
}

} // namespace

void CSearchIndexMaintenance::InsertBook(
    const Librova::Sqlite::CSqliteConnection& connection,
    const std::int64_t bookId,
    const Librova::Domain::SBookMetadata& metadata)
{
    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "INSERT INTO search_index(rowid, title, authors, tags, genres, description) VALUES(?, ?, ?, ?, ?, ?);");
    statement.BindInt64(1, bookId);
    statement.BindText(2, Librova::Domain::NormalizeText(metadata.TitleUtf8));
    statement.BindText(3, JoinNormalizedText(metadata.AuthorsUtf8));
    statement.BindText(4, JoinNormalizedText(metadata.TagsUtf8));
    statement.BindText(5, JoinNormalizedText(metadata.GenresUtf8));
    statement.BindText(6, BuildNormalizedDescription(metadata.DescriptionUtf8));
    static_cast<void>(statement.Step());
}

void CSearchIndexMaintenance::RemoveBook(
    const Librova::Sqlite::CSqliteConnection& connection,
    const std::int64_t bookId,
    const Librova::Domain::SBookMetadata& metadata)
{
    ExecuteDeleteCommand(connection, bookId, metadata);
}

void CSearchIndexMaintenance::RebuildAll(const Librova::Sqlite::CSqliteConnection& connection)
{
    if (!HasTable(connection, "search_index"))
    {
        return;
    }

    const std::vector<SBookSearchMetadata> books = ReadAllBooksForRebuild(connection);
    connection.Execute("INSERT INTO search_index(search_index) VALUES('delete-all');");

    for (const SBookSearchMetadata& book : books)
    {
        InsertBook(connection, book.BookId, book.Metadata);
    }
}

} // namespace Librova::SearchIndex
