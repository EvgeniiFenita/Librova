#include "SearchIndex/SearchIndexMaintenance.hpp"

#include <string>
#include <vector>

#include "Domain/MetadataNormalization.hpp"
#include "Sqlite/SqliteStatement.hpp"

namespace LibriFlow::SearchIndex {
namespace {

std::string JoinNormalizedText(const std::vector<std::string>& values)
{
    std::string joined;

    for (const std::string& value : values)
    {
        const std::string normalized = LibriFlow::Domain::NormalizeText(value);

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

    return LibriFlow::Domain::NormalizeText(*description);
}

void ExecuteDeleteCommand(
    const LibriFlow::Sqlite::CSqliteConnection& connection,
    const std::int64_t bookId,
    const LibriFlow::Domain::SBookMetadata& metadata)
{
    LibriFlow::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "INSERT INTO search_index(search_index, rowid, title, authors, tags, description) VALUES('delete', ?, ?, ?, ?, ?);");
    statement.BindInt64(1, bookId);
    statement.BindText(2, LibriFlow::Domain::NormalizeText(metadata.TitleUtf8));
    statement.BindText(3, JoinNormalizedText(metadata.AuthorsUtf8));
    statement.BindText(4, JoinNormalizedText(metadata.TagsUtf8));
    statement.BindText(5, BuildNormalizedDescription(metadata.DescriptionUtf8));
    static_cast<void>(statement.Step());
}

} // namespace

void CSearchIndexMaintenance::UpsertBook(
    const LibriFlow::Sqlite::CSqliteConnection& connection,
    const std::int64_t bookId,
    const LibriFlow::Domain::SBookMetadata& metadata)
{
    LibriFlow::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "INSERT INTO search_index(rowid, title, authors, tags, description) VALUES(?, ?, ?, ?, ?);");
    statement.BindInt64(1, bookId);
    statement.BindText(2, LibriFlow::Domain::NormalizeText(metadata.TitleUtf8));
    statement.BindText(3, JoinNormalizedText(metadata.AuthorsUtf8));
    statement.BindText(4, JoinNormalizedText(metadata.TagsUtf8));
    statement.BindText(5, BuildNormalizedDescription(metadata.DescriptionUtf8));
    static_cast<void>(statement.Step());
}

void CSearchIndexMaintenance::RemoveBook(
    const LibriFlow::Sqlite::CSqliteConnection& connection,
    const std::int64_t bookId,
    const LibriFlow::Domain::SBookMetadata& metadata)
{
    ExecuteDeleteCommand(connection, bookId, metadata);
}

} // namespace LibriFlow::SearchIndex
