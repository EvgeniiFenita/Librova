#pragma once

#include <cstdint>

#include "Domain/Book.hpp"
#include "Sqlite/SqliteConnection.hpp"

namespace LibriFlow::SearchIndex {

class CSearchIndexMaintenance final
{
public:
    static void UpsertBook(
        const LibriFlow::Sqlite::CSqliteConnection& connection,
        std::int64_t bookId,
        const LibriFlow::Domain::SBookMetadata& metadata);

    static void RemoveBook(
        const LibriFlow::Sqlite::CSqliteConnection& connection,
        std::int64_t bookId,
        const LibriFlow::Domain::SBookMetadata& metadata);
};

} // namespace LibriFlow::SearchIndex
