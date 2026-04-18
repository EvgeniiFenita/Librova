#include "Database/SqliteGenreHelpers.hpp"

#include <stdexcept>
#include <unordered_set>

#include "Domain/MetadataNormalization.hpp"
#include "Database/SqliteStatement.hpp"

namespace Librova::BookDatabase {

std::int64_t CSqliteGenreHelpers::ResolveGenreId(
    const Librova::Sqlite::CSqliteConnection& connection,
    std::string_view normalizedName,
    std::string_view displayName)
{
    {
        Librova::Sqlite::CSqliteStatement insertStatement(
            connection.GetNativeHandle(),
            "INSERT INTO genres (normalized_name, display_name) VALUES (?, ?) "
            "ON CONFLICT(normalized_name) DO NOTHING;");
        insertStatement.BindText(1, normalizedName);
        insertStatement.BindText(2, displayName);
        static_cast<void>(insertStatement.Step());
    }

    Librova::Sqlite::CSqliteStatement selectStatement(
        connection.GetNativeHandle(),
        "SELECT id FROM genres WHERE normalized_name = ?;");
    selectStatement.BindText(1, normalizedName);

    if (!selectStatement.Step())
    {
        throw std::runtime_error("Failed to resolve genre id after insert.");
    }

    return selectStatement.GetColumnInt64(0);
}

void CSqliteGenreHelpers::InsertGenres(
    const Librova::Sqlite::CSqliteConnection& connection,
    std::int64_t bookId,
    const std::vector<std::string>& genres,
    std::string_view sourceType)
{
    std::unordered_set<std::string> inserted;

    for (const std::string& genre : genres)
    {
        const std::string normalized = Librova::Domain::NormalizeText(genre);

        if (normalized.empty() || !inserted.insert(normalized).second)
        {
            continue;
        }

        const std::int64_t genreId = ResolveGenreId(connection, normalized, genre);

        Librova::Sqlite::CSqliteStatement linkStatement(
            connection.GetNativeHandle(),
            "INSERT OR IGNORE INTO book_genres (book_id, genre_id, source_type) VALUES (?, ?, ?);");
        linkStatement.BindInt64(1, bookId);
        linkStatement.BindInt64(2, genreId);
        linkStatement.BindText(3, sourceType);
        static_cast<void>(linkStatement.Step());
    }
}

} // namespace Librova::BookDatabase
