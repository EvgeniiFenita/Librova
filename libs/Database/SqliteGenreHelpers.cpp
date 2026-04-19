#include "Database/SqliteGenreHelpers.hpp"

#include <unordered_set>

#include "Database/SqliteEntityHelpers.hpp"
#include "Database/SqliteStatement.hpp"
#include "Domain/MetadataNormalization.hpp"

namespace Librova::BookDatabase {

std::int64_t CSqliteGenreHelpers::ResolveGenreId(
    const Librova::Sqlite::CSqliteConnection& connection,
    std::string_view normalizedName,
    std::string_view displayName)
{
    return Librova::Sqlite::ResolveEntityId(connection, "genres", normalizedName, displayName);
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
