#include "DatabaseRuntime/SchemaMigrator.hpp"

#include <format>

#include "DatabaseSchema/DatabaseSchema.hpp"
#include "Domain/MetadataNormalization.hpp"
#include "Sqlite/SqliteConnection.hpp"
#include "Sqlite/SqliteStatement.hpp"

namespace Librova::DatabaseRuntime {
namespace {

int ReadUserVersionValue(const Librova::Sqlite::CSqliteConnection& connection)
{
    Librova::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), "PRAGMA user_version;");

    if (!statement.Step())
    {
        return 0;
    }

    return statement.GetColumnInt(0);
}

bool HasColumn(
    const Librova::Sqlite::CSqliteConnection& connection,
    const std::string_view tableName,
    const std::string_view columnName)
{
    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        std::format("PRAGMA table_info({});", tableName));

    while (statement.Step())
    {
        if (statement.GetColumnText(1) == columnName)
        {
            return true;
        }
    }

    return false;
}

bool HasTable(
    const Librova::Sqlite::CSqliteConnection& connection,
    const std::string_view tableName)
{
    Librova::Sqlite::CSqliteStatement statement(
        connection.GetNativeHandle(),
        "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = ?;");
    statement.BindText(1, tableName);
    if (!statement.Step())
    {
        return false;
    }

    return statement.GetColumnInt(0) > 0;
}

void UpgradeToVersion2(const Librova::Sqlite::CSqliteConnection& connection)
{
    if (!HasColumn(connection, "books", "normalized_title"))
    {
        connection.Execute("ALTER TABLE books ADD COLUMN normalized_title TEXT;");
    }

    {
        Librova::Sqlite::CSqliteStatement selectStatement(
            connection.GetNativeHandle(),
            "SELECT id, title FROM books WHERE normalized_title IS NULL OR normalized_title = '';");

        while (selectStatement.Step())
        {
            Librova::Sqlite::CSqliteStatement updateStatement(
                connection.GetNativeHandle(),
                "UPDATE books SET normalized_title = ? WHERE id = ?;");
            updateStatement.BindText(1, Librova::Domain::NormalizeText(selectStatement.GetColumnText(1)));
            updateStatement.BindInt64(2, selectStatement.GetColumnInt64(0));
            static_cast<void>(updateStatement.Step());
        }
    }

    connection.Execute("CREATE INDEX IF NOT EXISTS idx_books_normalized_title ON books(normalized_title);");
}

void UpgradeToVersion3(const Librova::Sqlite::CSqliteConnection& connection)
{
    if (!HasColumn(connection, "books", "storage_encoding"))
    {
        connection.Execute("ALTER TABLE books ADD COLUMN storage_encoding TEXT NOT NULL DEFAULT 'plain';");
    }

    if (!HasTable(connection, "formats"))
    {
        connection.Execute(
            "CREATE TABLE formats ("
            "book_id INTEGER PRIMARY KEY,"
            "format TEXT NOT NULL,"
            "storage_encoding TEXT NOT NULL DEFAULT 'plain',"
            "managed_path TEXT NOT NULL,"
            "file_size_bytes INTEGER NOT NULL,"
            "sha256_hex TEXT NOT NULL,"
            "FOREIGN KEY (book_id) REFERENCES books(id) ON DELETE CASCADE"
            ");");
        connection.Execute(
            "INSERT INTO formats (book_id, format, storage_encoding, managed_path, file_size_bytes, sha256_hex) "
            "SELECT id, preferred_format, storage_encoding, managed_path, file_size_bytes, sha256_hex "
            "FROM books;");
        return;
    }

    if (!HasColumn(connection, "formats", "storage_encoding"))
    {
        connection.Execute("ALTER TABLE formats ADD COLUMN storage_encoding TEXT NOT NULL DEFAULT 'plain';");
    }
}

} // namespace

void CSchemaMigrator::Migrate(const std::filesystem::path& databasePath)
{
    Librova::Sqlite::CSqliteConnection connection(databasePath);

    const int currentVersion = ReadUserVersionValue(connection);
    const int expectedVersion = Librova::DatabaseSchema::CDatabaseSchema::GetCurrentVersion();

    if (currentVersion > expectedVersion)
    {
        throw std::runtime_error(
            std::format(
                "Database schema version {} is newer than this version of Librova supports ({}). "
                "Update Librova to open this library.",
                currentVersion,
                expectedVersion));
    }

    connection.Execute("PRAGMA foreign_keys = ON;");
    connection.Execute("PRAGMA journal_mode = WAL;");

    if (currentVersion == 0)
    {
        connection.Execute(Librova::DatabaseSchema::CDatabaseSchema::GetCreateSchemaScript());
    }

    if (currentVersion < 2)
    {
        UpgradeToVersion2(connection);
    }

    if (currentVersion < 3)
    {
        UpgradeToVersion3(connection);
    }

    connection.Execute(std::format("PRAGMA user_version = {};", Librova::DatabaseSchema::CDatabaseSchema::GetCurrentVersion()));
}

int CSchemaMigrator::ReadUserVersion(const std::filesystem::path& databasePath)
{
    Librova::Sqlite::CSqliteConnection connection(databasePath);
    return ReadUserVersionValue(connection);
}

} // namespace Librova::DatabaseRuntime
