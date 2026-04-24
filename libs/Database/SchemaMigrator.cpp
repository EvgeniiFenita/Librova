#include "Database/SchemaMigrator.hpp"

#include <format>
#include <stdexcept>

#include "Database/DatabaseSchema.hpp"
#include "Database/SqliteConnection.hpp"
#include "Database/SqliteStatement.hpp"

namespace Librova::DatabaseRuntime {
namespace {

void UpgradeToVersion2(const Librova::Sqlite::CSqliteConnection& connection)
{
    connection.Execute(Librova::DatabaseSchema::CDatabaseSchema::GetCollectionSchemaScript());
}

int ReadUserVersionValue(const Librova::Sqlite::CSqliteConnection& connection)
{
    Librova::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), "PRAGMA user_version;");

    if (!statement.Step())
    {
        return 0;
    }

    return statement.GetColumnInt(0);
}

} // namespace

void CSchemaMigrator::Migrate(const std::filesystem::path& databasePath)
{
    Librova::Sqlite::CSqliteConnection connection(databasePath);

    const int currentVersion = ReadUserVersionValue(connection);
    const int expectedVersion = Librova::DatabaseSchema::CDatabaseSchema::GetCurrentVersion();

    if (currentVersion == expectedVersion)
    {
        return;
    }

    if (currentVersion < 0 || currentVersion > expectedVersion)
    {
        throw std::runtime_error(
            std::format(
                "Database schema version {} is incompatible with this version of Librova "
                "(expected version {}).  Please delete the library database to recreate it.",
                currentVersion,
                expectedVersion));
    }

    connection.Execute("PRAGMA journal_mode = WAL;");
    connection.Execute("BEGIN IMMEDIATE;");

    try
    {
        if (currentVersion == 0)
        {
            connection.Execute(Librova::DatabaseSchema::CDatabaseSchema::GetCreateSchemaScript());
        }
        else
        {
            if (currentVersion < 2)
            {
                UpgradeToVersion2(connection);
            }
        }

        connection.Execute(std::format("PRAGMA user_version = {};", expectedVersion));
        connection.Execute("COMMIT;");
    }
    catch (...)
    {
        try { connection.Execute("ROLLBACK;"); } catch (...) {}
        throw;
    }
}

int CSchemaMigrator::ReadUserVersion(const std::filesystem::path& databasePath)
{
    Librova::Sqlite::CSqliteConnection connection(databasePath);
    return ReadUserVersionValue(connection);
}

} // namespace Librova::DatabaseRuntime
