#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "DatabaseRuntime/SchemaMigrator.hpp"
#include "Sqlite/SqliteConnection.hpp"
#include "Sqlite/SqliteStatement.hpp"

TEST_CASE("Schema migrator applies schema and sets user version", "[database-runtime]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "libriflow-schema-migrator.db";
    std::filesystem::remove(databasePath);

    LibriFlow::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    REQUIRE(LibriFlow::DatabaseRuntime::CSchemaMigrator::ReadUserVersion(databasePath) == 1);

    {
        LibriFlow::Sqlite::CSqliteConnection connection(databasePath);
        LibriFlow::Sqlite::CSqliteStatement statement(
            connection.GetNativeHandle(),
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = 'books';");

        REQUIRE(statement.Step());
        REQUIRE(statement.GetColumnInt(0) == 1);
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Schema migrator is idempotent for existing database", "[database-runtime]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "libriflow-schema-migrator-idempotent.db";
    std::filesystem::remove(databasePath);

    LibriFlow::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    LibriFlow::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    REQUIRE(LibriFlow::DatabaseRuntime::CSchemaMigrator::ReadUserVersion(databasePath) == 1);

    std::filesystem::remove(databasePath);
}
