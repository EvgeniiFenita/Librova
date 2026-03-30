#include "DatabaseRuntime/SchemaMigrator.hpp"

#include <format>

#include "DatabaseSchema/DatabaseSchema.hpp"
#include "Sqlite/SqliteConnection.hpp"
#include "Sqlite/SqliteStatement.hpp"

namespace Librova::DatabaseRuntime {

void CSchemaMigrator::Migrate(const std::filesystem::path& databasePath)
{
    Librova::Sqlite::CSqliteConnection connection(databasePath);

    for (const std::string_view statement : Librova::DatabaseSchema::CDatabaseSchema::GetMigrationStatements())
    {
        connection.Execute(statement);
    }

    connection.Execute(std::format("PRAGMA user_version = {};", Librova::DatabaseSchema::CDatabaseSchema::GetCurrentVersion()));
}

int CSchemaMigrator::ReadUserVersion(const std::filesystem::path& databasePath)
{
    Librova::Sqlite::CSqliteConnection connection(databasePath);
    Librova::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), "PRAGMA user_version;");

    if (!statement.Step())
    {
        return 0;
    }

    return statement.GetColumnInt(0);
}

} // namespace Librova::DatabaseRuntime
