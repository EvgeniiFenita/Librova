#include "BookDatabase/SqliteBookQueryRepository.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "BookDatabase/SqliteBookRepository.hpp"
#include "Domain/BookFormat.hpp"
#include "Domain/MetadataNormalization.hpp"
#include "Sqlite/SqliteConnection.hpp"
#include "Sqlite/SqliteStatement.hpp"

namespace LibriFlow::BookDatabase {
namespace {

std::string BuildFtsQuery(const std::string_view text)
{
    const std::string normalizedText = LibriFlow::Domain::NormalizeText(text);
    std::string query;
    std::string token;

    for (const char currentCharacter : normalizedText)
    {
        if (currentCharacter == ' ')
        {
            if (!token.empty())
            {
                if (!query.empty())
                {
                    query.push_back(' ');
                }

                query.append(token);
                query.push_back('*');
                token.clear();
            }

            continue;
        }

        token.push_back(currentCharacter);
    }

    if (!token.empty())
    {
        if (!query.empty())
        {
            query.push_back(' ');
        }

        query.append(token);
        query.push_back('*');
    }

    return query;
}

void BindTextFilters(LibriFlow::Sqlite::CSqliteStatement& statement, int& parameterIndex, const LibriFlow::Domain::SSearchQuery& query)
{
    if (query.HasText())
    {
        statement.BindText(parameterIndex++, BuildFtsQuery(query.TextUtf8));
    }

    if (query.AuthorUtf8.has_value())
    {
        statement.BindText(parameterIndex++, LibriFlow::Domain::NormalizeText(*query.AuthorUtf8));
    }

    if (query.Language.has_value())
    {
        statement.BindText(parameterIndex++, *query.Language);
    }

    if (query.SeriesUtf8.has_value())
    {
        statement.BindText(parameterIndex++, *query.SeriesUtf8);
    }

    for (const std::string& tag : query.TagsUtf8)
    {
        statement.BindText(parameterIndex++, LibriFlow::Domain::NormalizeText(tag));
    }

    if (query.Format.has_value())
    {
        statement.BindText(parameterIndex++, LibriFlow::Domain::ToString(*query.Format));
    }

    statement.BindInt64(parameterIndex++, static_cast<std::int64_t>(query.Limit));
    statement.BindInt64(parameterIndex, static_cast<std::int64_t>(query.Offset));
}

std::string BuildSearchSql(const LibriFlow::Domain::SSearchQuery& query)
{
    std::string sql =
        "SELECT DISTINCT b.id "
        "FROM books b ";

    if (query.HasText())
    {
        sql += "INNER JOIN search_index si ON si.rowid = b.id ";
    }

    if (query.AuthorUtf8.has_value())
    {
        sql +=
            "INNER JOIN book_authors ba_filter ON ba_filter.book_id = b.id "
            "INNER JOIN authors a_filter ON a_filter.id = ba_filter.author_id ";
    }

    sql += "WHERE 1 = 1 ";

    if (query.HasText())
    {
        sql += "AND si MATCH ? ";
    }

    if (query.AuthorUtf8.has_value())
    {
        sql += "AND a_filter.normalized_name = ? ";
    }

    if (query.Language.has_value())
    {
        sql += "AND b.language = ? ";
    }

    if (query.SeriesUtf8.has_value())
    {
        sql += "AND b.series = ? ";
    }

    for ([[maybe_unused]] const std::string& tag : query.TagsUtf8)
    {
        sql +=
            "AND EXISTS ("
            "SELECT 1 FROM book_tags bt_filter "
            "INNER JOIN tags t_filter ON t_filter.id = bt_filter.tag_id "
            "WHERE bt_filter.book_id = b.id AND t_filter.normalized_name = ?"
            ") ";
    }

    if (query.Format.has_value())
    {
        sql += "AND b.preferred_format = ? ";
    }

    switch (query.SortBy.value_or(LibriFlow::Domain::EBookSort::Title))
    {
    case LibriFlow::Domain::EBookSort::Title:
        sql += "ORDER BY b.title COLLATE NOCASE ASC ";
        break;
    case LibriFlow::Domain::EBookSort::Author:
        sql +=
            "ORDER BY ("
            "SELECT MIN(a_sort.display_name) "
            "FROM book_authors ba_sort "
            "INNER JOIN authors a_sort ON a_sort.id = ba_sort.author_id "
            "WHERE ba_sort.book_id = b.id"
            ") COLLATE NOCASE ASC, b.title COLLATE NOCASE ASC ";
        break;
    case LibriFlow::Domain::EBookSort::DateAdded:
        sql += "ORDER BY b.added_at_utc DESC, b.title COLLATE NOCASE ASC ";
        break;
    case LibriFlow::Domain::EBookSort::Series:
        sql += "ORDER BY b.series COLLATE NOCASE ASC, b.series_index ASC, b.title COLLATE NOCASE ASC ";
        break;
    case LibriFlow::Domain::EBookSort::Year:
        sql += "ORDER BY b.year DESC, b.title COLLATE NOCASE ASC ";
        break;
    case LibriFlow::Domain::EBookSort::FileSize:
        sql += "ORDER BY b.file_size_bytes DESC, b.title COLLATE NOCASE ASC ";
        break;
    }

    sql += "LIMIT ? OFFSET ?;";
    return sql;
}

void AppendStrictDuplicateMatches(
    std::vector<LibriFlow::Domain::SDuplicateMatch>& matches,
    std::unordered_set<std::int64_t>& seenIds,
    const std::filesystem::path& databasePath,
    const std::string_view sql,
    const std::string_view value,
    const LibriFlow::Domain::EDuplicateReason reason)
{
    LibriFlow::Sqlite::CSqliteConnection connection(databasePath);
    LibriFlow::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), sql);
    statement.BindText(1, value);

    while (statement.Step())
    {
        const std::int64_t existingId = statement.GetColumnInt64(0);

        if (!seenIds.insert(existingId).second)
        {
            continue;
        }

        matches.push_back({
            .Severity = LibriFlow::Domain::EDuplicateSeverity::Strict,
            .Reason = reason,
            .ExistingBookId = LibriFlow::Domain::SBookId{existingId}
        });
    }
}

} // namespace

CSqliteBookQueryRepository::CSqliteBookQueryRepository(std::filesystem::path databasePath)
    : m_databasePath(std::move(databasePath))
{
}

std::vector<LibriFlow::Domain::SBook> CSqliteBookQueryRepository::Search(const LibriFlow::Domain::SSearchQuery& query) const
{
    LibriFlow::Sqlite::CSqliteConnection connection(m_databasePath);
    LibriFlow::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), BuildSearchSql(query));

    int parameterIndex = 1;
    BindTextFilters(statement, parameterIndex, query);

    CSqliteBookRepository repository(m_databasePath);
    std::vector<LibriFlow::Domain::SBook> books;

    while (statement.Step())
    {
        const LibriFlow::Domain::SBookId bookId{statement.GetColumnInt64(0)};
        const std::optional<LibriFlow::Domain::SBook> book = repository.GetById(bookId);

        if (book.has_value())
        {
            books.push_back(*book);
        }
    }

    return books;
}

std::vector<LibriFlow::Domain::SDuplicateMatch> CSqliteBookQueryRepository::FindDuplicates(const LibriFlow::Domain::SCandidateBook& candidate) const
{
    std::vector<LibriFlow::Domain::SDuplicateMatch> matches;
    std::unordered_set<std::int64_t> seenIds;

    if (candidate.HasHash())
    {
        AppendStrictDuplicateMatches(
            matches,
            seenIds,
            m_databasePath,
            "SELECT id FROM books WHERE sha256_hex = ?;",
            *candidate.Sha256Hex,
            LibriFlow::Domain::EDuplicateReason::SameHash);
    }

    if (candidate.HasIsbn())
    {
        const std::optional<std::string> normalizedIsbn = LibriFlow::Domain::NormalizeIsbn(candidate.Metadata.Isbn);

        if (normalizedIsbn.has_value())
        {
            AppendStrictDuplicateMatches(
                matches,
                seenIds,
                m_databasePath,
                "SELECT id FROM books WHERE isbn = ?;",
                *normalizedIsbn,
                LibriFlow::Domain::EDuplicateReason::SameIsbn);
        }
    }

    if (candidate.Metadata.HasTitle() && candidate.Metadata.HasAuthors())
    {
        const std::string candidateDuplicateKey = LibriFlow::Domain::BuildDuplicateKey(candidate.Metadata);

        if (!candidateDuplicateKey.empty())
        {
            LibriFlow::Sqlite::CSqliteConnection connection(m_databasePath);
            LibriFlow::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), "SELECT id FROM books;");
            CSqliteBookRepository repository(m_databasePath);

            while (statement.Step())
            {
                const LibriFlow::Domain::SBookId bookId{statement.GetColumnInt64(0)};

                if (seenIds.contains(bookId.Value))
                {
                    continue;
                }

                const std::optional<LibriFlow::Domain::SBook> existingBook = repository.GetById(bookId);

                if (!existingBook.has_value())
                {
                    continue;
                }

                if (LibriFlow::Domain::BuildDuplicateKey(existingBook->Metadata) == candidateDuplicateKey)
                {
                    matches.push_back({
                        .Severity = LibriFlow::Domain::EDuplicateSeverity::Probable,
                        .Reason = LibriFlow::Domain::EDuplicateReason::SameNormalizedTitleAndAuthors,
                        .ExistingBookId = bookId
                    });
                    seenIds.insert(bookId.Value);
                }
            }
        }
    }

    return matches;
}

} // namespace LibriFlow::BookDatabase
