#include "DatabaseRuntime/SchemaMigrator.hpp"

#include <format>
#include <stdexcept>

#include "DatabaseSchema/DatabaseSchema.hpp"
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

    if (currentVersion != 0)
    {
        // Database schema is incompatible with this version of Librova.
        // Forward-compatibility (newer DB, older binary) and backward-compat
        // (older DB without migration) are both rejected.  The user must
        // delete the library database so Librova can recreate it.
        throw std::runtime_error(
            std::format(
                "Database schema version {} is incompatible with this version of Librova "
                "(expected version {}).  Please delete the library database to recreate it.",
                currentVersion,
                expectedVersion));
    }

    // currentVersion == 0: brand-new database — apply the full schema.
    // -----------------------------------------------------------------------
    // Future migrations: when the schema must change and existing databases
    // need upgrading (rather than recreation), relax the incompatibility check
    // above and add upgrade functions here, e.g.:
    //
    //     if (currentVersion < 2) { UpgradeToVersion2(connection); }
    //     if (currentVersion < 3) { UpgradeToVersion3(connection); }
    //
    // Each UpgradeToVersionN operates inside the transaction opened below.
    // Remember to bump GetCurrentVersion() in DatabaseSchema.cpp accordingly.
    // -----------------------------------------------------------------------
    connection.Execute("PRAGMA foreign_keys = ON;");
    connection.Execute("PRAGMA journal_mode = WAL;");
    connection.Execute("BEGIN IMMEDIATE;");

    try
    {
        connection.Execute(Librova::DatabaseSchema::CDatabaseSchema::GetCreateSchemaScript());
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
