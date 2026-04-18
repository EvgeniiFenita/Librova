#pragma once

#include <cstdint>

#include "Domain/Book.hpp"
#include "Database/SqliteConnection.hpp"

namespace Librova::SearchIndex {

class CSearchIndexMaintenance final
{
public:
    static void InsertBook(
        const Librova::Sqlite::CSqliteConnection& connection,
        std::int64_t bookId,
        const Librova::Domain::SBookMetadata& metadata);

    static void RemoveBook(
        const Librova::Sqlite::CSqliteConnection& connection,
        std::int64_t bookId,
        const Librova::Domain::SBookMetadata& metadata);

    // Drops all rows from search_index and re-inserts from books, authors,
    // book_tags/tags, and book_genres/genres.  Reads genres only if those
    // tables are present (safe to call on databases that pre-date genres support).
    static void RebuildAll(const Librova::Sqlite::CSqliteConnection& connection);
};

} // namespace Librova::SearchIndex
