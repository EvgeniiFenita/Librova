#include "BookDatabase/SqliteBookQueryRepository.hpp"

#include <chrono>
#include <format>
#include <optional>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Domain/BookFormat.hpp"
#include "Domain/MetadataNormalization.hpp"
#include "Domain/StorageEncoding.hpp"
#include "Sqlite/SqliteConnection.hpp"
#include "Sqlite/SqliteStatement.hpp"
#include "Unicode/UnicodeConversion.hpp"

namespace Librova::BookDatabase {
namespace {

std::string BuildFtsQuery(const std::string_view text)
{
    const std::string normalizedText = Librova::Domain::NormalizeText(text);
    std::string sanitizedText;
    sanitizedText.reserve(normalizedText.size());

    for (const unsigned char currentCharacter : normalizedText)
    {
        if (currentCharacter == ' ')
        {
            sanitizedText.push_back(' ');
            continue;
        }

        if (currentCharacter >= 0x80 || std::isalnum(currentCharacter))
        {
            sanitizedText.push_back(static_cast<char>(currentCharacter));
            continue;
        }

        sanitizedText.push_back(' ');
    }

    const std::string tokenizedText = Librova::Domain::NormalizeText(sanitizedText);
    std::string query;
    std::string token;

    for (const char currentCharacter : tokenizedText)
    {
        if (currentCharacter == ' ')
        {
            if (!token.empty())
            {
                if (!query.empty())
                {
                    query.push_back(' ');
                }

                query.push_back('"');
                query.append(token);
                query.append("\"*");
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

        query.push_back('"');
        query.append(token);
        query.append("\"*");
    }

    return query;
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

std::string BuildIdInClause(const std::size_t count)
{
    std::string sql = "(";

    for (std::size_t index = 0; index < count; ++index)
    {
        if (index > 0)
        {
            sql += ", ";
        }

        sql += "?";
    }

    sql += ")";
    return sql;
}

void BindBookIds(Librova::Sqlite::CSqliteStatement& statement, const std::vector<std::int64_t>& bookIds, int firstParameterIndex = 1)
{
    int parameterIndex = firstParameterIndex;

    for (const std::int64_t bookId : bookIds)
    {
        statement.BindInt64(parameterIndex++, bookId);
    }
}

std::unordered_map<std::int64_t, std::vector<std::string>> ReadAuthorsByBookId(
    const Librova::Sqlite::CSqliteConnection& connection,
    const std::vector<std::int64_t>& bookIds)
{
    if (bookIds.empty())
    {
        return {};
    }

    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        std::format(
            "SELECT ba.book_id, a.display_name "
            "FROM book_authors ba "
            "INNER JOIN authors a ON a.id = ba.author_id "
            "WHERE ba.book_id IN {} "
            "ORDER BY ba.book_id ASC, ba.author_order ASC;",
            BuildIdInClause(bookIds.size())));
    BindBookIds(statement, bookIds);

    std::unordered_map<std::int64_t, std::vector<std::string>> authorsByBookId;

    while (statement.Step())
    {
        authorsByBookId[statement.GetColumnInt64(0)].push_back(statement.GetColumnText(1));
    }

    return authorsByBookId;
}

std::unordered_map<std::int64_t, std::vector<std::string>> ReadTagsByBookId(
    const Librova::Sqlite::CSqliteConnection& connection,
    const std::vector<std::int64_t>& bookIds)
{
    if (bookIds.empty())
    {
        return {};
    }

    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        std::format(
            "SELECT bt.book_id, t.display_name "
            "FROM book_tags bt "
            "INNER JOIN tags t ON t.id = bt.tag_id "
            "WHERE bt.book_id IN {} "
            "ORDER BY bt.book_id ASC, t.display_name ASC;",
            BuildIdInClause(bookIds.size())));
    BindBookIds(statement, bookIds);

    std::unordered_map<std::int64_t, std::vector<std::string>> tagsByBookId;

    while (statement.Step())
    {
        tagsByBookId[statement.GetColumnInt64(0)].push_back(statement.GetColumnText(1));
    }

    return tagsByBookId;
}

std::unordered_map<std::int64_t, Librova::Domain::SBook> ReadBooksById(
    const Librova::Sqlite::CSqliteConnection& connection,
    const std::vector<std::int64_t>& bookIds)
{
    if (bookIds.empty())
    {
        return {};
    }

    const auto authorsByBookId = ReadAuthorsByBookId(connection, bookIds);
    const auto tagsByBookId = ReadTagsByBookId(connection, bookIds);

    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        std::format(
            "SELECT id, title, language, series, series_index, publisher, year, isbn, description, identifier, preferred_format, storage_encoding, managed_path, cover_path, file_size_bytes, sha256_hex, added_at_utc "
            "FROM books WHERE id IN {};",
            BuildIdInClause(bookIds.size())));
    BindBookIds(statement, bookIds);

    std::unordered_map<std::int64_t, Librova::Domain::SBook> booksById;
    booksById.reserve(bookIds.size());

    while (statement.Step())
    {
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
        if (const auto authorIterator = authorsByBookId.find(book.Id.Value); authorIterator != authorsByBookId.end())
        {
            book.Metadata.AuthorsUtf8 = authorIterator->second;
        }

        if (const auto tagIterator = tagsByBookId.find(book.Id.Value); tagIterator != tagsByBookId.end())
        {
            book.Metadata.TagsUtf8 = tagIterator->second;
        }

        book.File.Format = *format;
        book.File.StorageEncoding = *storageEncoding;
        book.File.ManagedPath = Librova::Unicode::PathFromUtf8(statement.GetColumnText(12));
        book.CoverPath = statement.IsColumnNull(13)
            ? std::nullopt
            : std::make_optional(Librova::Unicode::PathFromUtf8(statement.GetColumnText(13)));
        book.File.SizeBytes = static_cast<std::uintmax_t>(statement.GetColumnInt64(14));
        book.File.Sha256Hex = statement.GetColumnText(15);
        book.AddedAtUtc = ParseTimePoint(statement.GetColumnText(16));

        booksById.emplace(book.Id.Value, std::move(book));
    }

    return booksById;
}

void BindTextFilters(Librova::Sqlite::CSqliteStatement& statement, int& parameterIndex, const Librova::Domain::SSearchQuery& query)
{
    if (query.HasText())
    {
        statement.BindText(parameterIndex++, BuildFtsQuery(query.TextUtf8));
    }

    if (query.AuthorUtf8.has_value())
    {
        statement.BindText(parameterIndex++, Librova::Domain::NormalizeText(*query.AuthorUtf8));
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
        statement.BindText(parameterIndex++, Librova::Domain::NormalizeText(tag));
    }

    if (query.Format.has_value())
    {
        statement.BindText(parameterIndex++, Librova::Domain::ToString(*query.Format));
    }

    statement.BindInt64(parameterIndex++, static_cast<std::int64_t>(query.Limit));
    statement.BindInt64(parameterIndex, static_cast<std::int64_t>(query.Offset));
}

std::string BuildSearchSql(const Librova::Domain::SSearchQuery& query)
{
    std::string sql =
        "SELECT DISTINCT b.id "
        "FROM books b ";

    if (query.HasText())
    {
        sql += "INNER JOIN search_index ON search_index.rowid = b.id ";
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
        sql += "AND search_index MATCH ? ";
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

    const std::string_view dir =
        query.SortDirection == Librova::Domain::ESortDirection::Descending ? "DESC" : "ASC";

    switch (query.SortBy.value_or(Librova::Domain::EBookSort::Title))
    {
    case Librova::Domain::EBookSort::Title:
        sql += "ORDER BY b.title COLLATE NOCASE ";
        sql += dir;
        sql += " ";
        break;
    case Librova::Domain::EBookSort::Author:
        sql +=
            "ORDER BY ("
            "SELECT MIN(a_sort.display_name) "
            "FROM book_authors ba_sort "
            "INNER JOIN authors a_sort ON a_sort.id = ba_sort.author_id "
            "WHERE ba_sort.book_id = b.id"
            ") COLLATE NOCASE ";
        sql += dir;
        sql += ", b.title COLLATE NOCASE ASC ";
        break;
    case Librova::Domain::EBookSort::DateAdded:
        sql += "ORDER BY b.added_at_utc ";
        sql += dir;
        sql += ", b.title COLLATE NOCASE ASC ";
        break;
    case Librova::Domain::EBookSort::Series:
        sql += "ORDER BY b.series COLLATE NOCASE ASC, b.series_index ASC, b.title COLLATE NOCASE ASC ";
        break;
    case Librova::Domain::EBookSort::Year:
        sql += "ORDER BY b.year DESC, b.title COLLATE NOCASE ASC ";
        break;
    case Librova::Domain::EBookSort::FileSize:
        sql += "ORDER BY b.file_size_bytes DESC, b.title COLLATE NOCASE ASC ";
        break;
    }

    sql += "LIMIT ? OFFSET ?;";
    return sql;
}

std::string BuildSearchCountSql(const Librova::Domain::SSearchQuery& query)
{
    std::string sql =
        "SELECT COUNT(DISTINCT b.id) "
        "FROM books b ";

    if (query.HasText())
    {
        sql += "INNER JOIN search_index ON search_index.rowid = b.id ";
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
        sql += "AND search_index MATCH ? ";
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

    sql += ";";
    return sql;
}

std::string BuildAvailableLanguagesSql(const Librova::Domain::SSearchQuery& query)
{
    std::string sql =
        "SELECT DISTINCT b.language "
        "FROM books b ";

    if (query.HasText())
    {
        sql += "INNER JOIN search_index ON search_index.rowid = b.id ";
    }

    if (query.AuthorUtf8.has_value())
    {
        sql +=
            "INNER JOIN book_authors ba_filter ON ba_filter.book_id = b.id "
            "INNER JOIN authors a_filter ON a_filter.id = ba_filter.author_id ";
    }

    sql += "WHERE b.language <> '' ";

    if (query.HasText())
    {
        sql += "AND search_index MATCH ? ";
    }

    if (query.AuthorUtf8.has_value())
    {
        sql += "AND a_filter.normalized_name = ? ";
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

    sql += "ORDER BY b.language COLLATE NOCASE ASC;";
    return sql;
}

void BindSearchCountFilters(
    Librova::Sqlite::CSqliteStatement& statement,
    const Librova::Domain::SSearchQuery& query)
{
    int parameterIndex = 1;

    if (query.HasText())
    {
        statement.BindText(parameterIndex++, BuildFtsQuery(query.TextUtf8));
    }

    if (query.AuthorUtf8.has_value())
    {
        statement.BindText(parameterIndex++, Librova::Domain::NormalizeText(*query.AuthorUtf8));
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
        statement.BindText(parameterIndex++, Librova::Domain::NormalizeText(tag));
    }

    if (query.Format.has_value())
    {
        statement.BindText(parameterIndex, Librova::Domain::ToString(*query.Format));
    }
}

void BindAvailableLanguageFilters(
    Librova::Sqlite::CSqliteStatement& statement,
    const Librova::Domain::SSearchQuery& query)
{
    int parameterIndex = 1;

    if (query.HasText())
    {
        statement.BindText(parameterIndex++, BuildFtsQuery(query.TextUtf8));
    }

    if (query.AuthorUtf8.has_value())
    {
        statement.BindText(parameterIndex++, Librova::Domain::NormalizeText(*query.AuthorUtf8));
    }

    if (query.SeriesUtf8.has_value())
    {
        statement.BindText(parameterIndex++, *query.SeriesUtf8);
    }

    for (const std::string& tag : query.TagsUtf8)
    {
        statement.BindText(parameterIndex++, Librova::Domain::NormalizeText(tag));
    }

    if (query.Format.has_value())
    {
        statement.BindText(parameterIndex, Librova::Domain::ToString(*query.Format));
    }
}

void AppendStrictDuplicateMatches(
    std::vector<Librova::Domain::SDuplicateMatch>& matches,
    std::unordered_set<std::int64_t>& seenIds,
    const Librova::Sqlite::CSqliteConnection& connection,
    const std::string_view sql,
    const std::string_view value,
    const Librova::Domain::EDuplicateReason reason)
{
    Librova::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), sql);
    statement.BindText(1, value);

    while (statement.Step())
    {
        const std::int64_t existingId = statement.GetColumnInt64(0);

        if (!seenIds.insert(existingId).second)
        {
            continue;
        }

        matches.push_back({
            .Severity = Librova::Domain::EDuplicateSeverity::Strict,
            .Reason = reason,
            .ExistingBookId = Librova::Domain::SBookId{existingId}
        });
    }
}

std::vector<std::string> BuildNormalizedAuthors(const std::vector<std::string>& authors)
{
    std::vector<std::string> normalizedAuthors;
    normalizedAuthors.reserve(authors.size());

    for (const std::string& author : authors)
    {
        const std::string normalizedAuthor = Librova::Domain::NormalizeText(author);

        if (!normalizedAuthor.empty())
        {
            normalizedAuthors.push_back(normalizedAuthor);
        }
    }

    std::sort(normalizedAuthors.begin(), normalizedAuthors.end());
    return normalizedAuthors;
}

std::string BuildNormalizedTitle(const std::string& title)
{
    return Librova::Domain::NormalizeText(title);
}

std::filesystem::path ResolveLibraryRoot(const std::filesystem::path& databasePath)
{
    const std::filesystem::path databaseDirectory = databasePath.parent_path();

    if (databaseDirectory.filename() == std::filesystem::path("Database"))
    {
        return databaseDirectory.parent_path();
    }

    return databaseDirectory;
}

std::uint64_t GetFileSizeOrZero(const std::filesystem::path& path)
{
    std::error_code errorCode;
    const std::uintmax_t size = std::filesystem::file_size(path, errorCode);

    if (errorCode)
    {
        return 0;
    }

    return static_cast<std::uint64_t>(size);
}

std::uint64_t GetDirectoryFileSizeRecursiveOrZero(const std::filesystem::path& root)
{
    std::error_code errorCode;

    if (!std::filesystem::exists(root, errorCode))
    {
        return 0;
    }

    std::uint64_t totalSize = 0;
    std::filesystem::recursive_directory_iterator iterator(
        root,
        std::filesystem::directory_options::skip_permission_denied,
        errorCode);
    std::filesystem::recursive_directory_iterator end;

    if (errorCode)
    {
        return 0;
    }

    while (iterator != end)
    {
        if (iterator->is_regular_file(errorCode))
        {
            totalSize += GetFileSizeOrZero(iterator->path());
        }

        errorCode.clear();
        iterator.increment(errorCode);
        if (errorCode)
        {
            break;
        }
    }

    return totalSize;
}

std::vector<std::int64_t> FindProbableDuplicateCandidateIds(
    const Librova::Sqlite::CSqliteConnection& connection,
    const Librova::Domain::SCandidateBook& candidate)
{
    const std::string normalizedTitle = BuildNormalizedTitle(candidate.Metadata.TitleUtf8);
    const std::vector<std::string> normalizedAuthors = BuildNormalizedAuthors(candidate.Metadata.AuthorsUtf8);

    if (normalizedTitle.empty() || normalizedAuthors.empty())
    {
        return {};
    }

    if (std::adjacent_find(normalizedAuthors.begin(), normalizedAuthors.end()) != normalizedAuthors.end())
    {
        return {};
    }

    std::string sql =
        "SELECT ba.book_id "
        "FROM book_authors ba "
        "INNER JOIN authors a ON a.id = ba.author_id "
        "INNER JOIN books b ON b.id = ba.book_id "
        "WHERE a.normalized_name IN ";
    sql += BuildIdInClause(normalizedAuthors.size());
    sql +=
        " AND b.normalized_title = ? "
        " GROUP BY ba.book_id "
        "HAVING COUNT(*) = ? "
        "AND COUNT(*) = ("
        "SELECT COUNT(*) FROM book_authors ba_total WHERE ba_total.book_id = ba.book_id"
        ");";

    Librova::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), sql);

    int parameterIndex = 1;
    for (const std::string& normalizedAuthor : normalizedAuthors)
    {
        statement.BindText(parameterIndex++, normalizedAuthor);
    }

    statement.BindText(parameterIndex++, normalizedTitle);
    statement.BindInt64(parameterIndex, static_cast<std::int64_t>(normalizedAuthors.size()));

    std::vector<std::int64_t> bookIds;
    while (statement.Step())
    {
        bookIds.push_back(statement.GetColumnInt64(0));
    }

    return bookIds;
}

} // namespace

CSqliteBookQueryRepository::CSqliteBookQueryRepository(std::filesystem::path databasePath)
    : m_databasePath(std::move(databasePath))
{
}

std::vector<Librova::Domain::SBook> CSqliteBookQueryRepository::Search(const Librova::Domain::SSearchQuery& query) const
{
    Librova::Sqlite::CSqliteConnection connection(m_databasePath);
    Librova::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), BuildSearchSql(query));

    int parameterIndex = 1;
    BindTextFilters(statement, parameterIndex, query);

    std::vector<std::int64_t> bookIds;

    while (statement.Step())
    {
        bookIds.push_back(statement.GetColumnInt64(0));
    }

    const auto booksById = ReadBooksById(connection, bookIds);
    std::vector<Librova::Domain::SBook> books;
    books.reserve(bookIds.size());

    for (const std::int64_t bookId : bookIds)
    {
        if (const auto bookIterator = booksById.find(bookId); bookIterator != booksById.end())
        {
            books.push_back(bookIterator->second);
        }
    }

    return books;
}

std::uint64_t CSqliteBookQueryRepository::CountSearchResults(const Librova::Domain::SSearchQuery& query) const
{
    Librova::Sqlite::CSqliteConnection connection(m_databasePath);
    Librova::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), BuildSearchCountSql(query));
    BindSearchCountFilters(statement, query);

    if (!statement.Step())
    {
        return 0;
    }

    return static_cast<std::uint64_t>(statement.GetColumnInt64(0));
}

std::vector<std::string> CSqliteBookQueryRepository::ListAvailableLanguages(const Librova::Domain::SSearchQuery& query) const
{
    Librova::Sqlite::CSqliteConnection connection(m_databasePath);
    Librova::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), BuildAvailableLanguagesSql(query));
    BindAvailableLanguageFilters(statement, query);

    std::vector<std::string> languages;
    while (statement.Step())
    {
        languages.push_back(statement.GetColumnText(0));
    }

    return languages;
}

std::vector<Librova::Domain::SDuplicateMatch> CSqliteBookQueryRepository::FindDuplicates(const Librova::Domain::SCandidateBook& candidate) const
{
    std::vector<Librova::Domain::SDuplicateMatch> matches;
    std::unordered_set<std::int64_t> seenIds;
    Librova::Sqlite::CSqliteConnection connection(m_databasePath);

    if (candidate.HasHash())
    {
        AppendStrictDuplicateMatches(
            matches,
            seenIds,
            connection,
            "SELECT id FROM books WHERE sha256_hex = ?;",
            *candidate.Sha256Hex,
            Librova::Domain::EDuplicateReason::SameHash);
    }

    if (candidate.HasIsbn())
    {
        const std::optional<std::string> normalizedIsbn = Librova::Domain::NormalizeIsbn(candidate.Metadata.Isbn);

        if (normalizedIsbn.has_value())
        {
            AppendStrictDuplicateMatches(
                matches,
                seenIds,
                connection,
                "SELECT id FROM books WHERE isbn = ?;",
                *normalizedIsbn,
                Librova::Domain::EDuplicateReason::SameIsbn);
        }
    }

    if (candidate.Metadata.HasTitle() && candidate.Metadata.HasAuthors())
    {
        const std::vector<std::int64_t> probableCandidateIds = FindProbableDuplicateCandidateIds(connection, candidate);

        for (const std::int64_t probableCandidateId : probableCandidateIds)
        {
            const Librova::Domain::SBookId bookId{probableCandidateId};

            if (seenIds.contains(bookId.Value))
            {
                continue;
            }

            matches.push_back({
                .Severity = Librova::Domain::EDuplicateSeverity::Probable,
                .Reason = Librova::Domain::EDuplicateReason::SameNormalizedTitleAndAuthors,
                .ExistingBookId = bookId
            });
            seenIds.insert(bookId.Value);
        }
    }

    return matches;
}

Librova::Domain::IBookQueryRepository::SLibraryStatistics CSqliteBookQueryRepository::GetLibraryStatistics() const
{
    Librova::Sqlite::CSqliteConnection connection(m_databasePath);
    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "SELECT COUNT(*), COALESCE(SUM(file_size_bytes), 0) FROM books;");

    if (!statement.Step())
    {
        return {};
    }

    const std::uint64_t totalManagedBookSizeBytes = static_cast<std::uint64_t>(statement.GetColumnInt64(1));
    const std::filesystem::path libraryRoot = ResolveLibraryRoot(m_databasePath);
    const std::uint64_t totalCoverSizeBytes = GetDirectoryFileSizeRecursiveOrZero(libraryRoot / "Covers");
    const std::uint64_t databaseSizeBytes = GetFileSizeOrZero(m_databasePath);

    return {
        .BookCount = static_cast<std::uint64_t>(statement.GetColumnInt64(0)),
        .TotalManagedBookSizeBytes = totalManagedBookSizeBytes,
        .TotalLibrarySizeBytes = totalManagedBookSizeBytes + totalCoverSizeBytes + databaseSizeBytes
    };
}

} // namespace Librova::BookDatabase
