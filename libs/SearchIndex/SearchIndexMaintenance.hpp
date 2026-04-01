#pragma once

#include <cstdint>

#include "Domain/Book.hpp"
#include "Sqlite/SqliteConnection.hpp"

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
};

} // namespace Librova::SearchIndex
