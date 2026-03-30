#include "DatabaseRuntime/SchemaMigrator.hpp"

#include <format>

#include "DatabaseSchema/DatabaseSchema.hpp"
#include "Sqlite/SqliteConnection.hpp"
#include "Sqlite/SqliteStatement.hpp"

namespace LibriFlow::DatabaseRuntime {

void CSchemaMigrator::Migrate(const std::filesystem::path& databasePath)
{
    LibriFlow::Sqlite::CSqliteConnection connection(databasePath);

    for (const std::string_view statement : LibriFlow::DatabaseSchema::CDatabaseSchema::GetMigrationStatements())
    {
        connection.Execute(statement);
    }

    connection.Execute(std::format("PRAGMA user_version = {};", LibriFlow::DatabaseSchema::CDatabaseSchema::GetCurrentVersion()));
}

int CSchemaMigrator::ReadUserVersion(const std::filesystem::path& databasePath)
{
    LibriFlow::Sqlite::CSqliteConnection connection(databasePath);
    LibriFlow::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), "PRAGMA user_version;");

    if (!statement.Step())
    {
        return 0;
    }

    return statement.GetColumnInt(0);
}

} // namespace LibriFlow::DatabaseRuntime
