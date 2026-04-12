#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdint>
#include <filesystem>

#include "DatabaseRuntime/SchemaMigrator.hpp"
#include "Sqlite/SqliteConnection.hpp"
#include "Sqlite/SqliteStatement.hpp"

TEST_CASE("Schema migrator applies schema and sets user version", "[database-runtime]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-schema-migrator.db";
    std::filesystem::remove(databasePath);

    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    REQUIRE(Librova::DatabaseRuntime::CSchemaMigrator::ReadUserVersion(databasePath) == 1);

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);
        Librova::Sqlite::CSqliteStatement statement(
            connection.GetNativeHandle(),
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = 'books';");

        REQUIRE(statement.Step());
        REQUIRE(statement.GetColumnInt(0) == 1);
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Schema migrator is idempotent for existing database", "[database-runtime]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-schema-migrator-idempotent.db";
    std::filesystem::remove(databasePath);

    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    REQUIRE(Librova::DatabaseRuntime::CSchemaMigrator::ReadUserVersion(databasePath) == 1);

    std::filesystem::remove(databasePath);
}

TEST_CASE("Schema migrator rejects database with a schema version newer than the current binary supports", "[database-runtime]")
{
    const auto databasePath = std::filesystem::temp_directory_path() / "librova-schema-migrator-future.db";
    std::filesystem::remove(databasePath);

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);
        connection.Execute("PRAGMA user_version = 9999;");
    }

    REQUIRE_THROWS_WITH(
        Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath),
        Catch::Matchers::ContainsSubstring("9999") && Catch::Matchers::ContainsSubstring("1"));

    // user_version must NOT have been downgraded — the database must remain untouched
    REQUIRE(Librova::DatabaseRuntime::CSchemaMigrator::ReadUserVersion(databasePath) == 9999);

    std::filesystem::remove(databasePath);
}

TEST_CASE("Schema migrator rejects database with incompatible older schema version", "[database-runtime]")
{
    // Databases created by earlier Librova versions have schema versions 2–5.
    // Librova no longer provides upgrade paths for them and requires recreation.
    const auto databasePath = std::filesystem::temp_directory_path() / "librova-schema-migrator-old.db";
    std::filesystem::remove(databasePath);

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);
        connection.Execute("PRAGMA user_version = 2;");
    }

    REQUIRE_THROWS_WITH(
        Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath),
        Catch::Matchers::ContainsSubstring("2") && Catch::Matchers::ContainsSubstring("1"));

    // user_version must NOT have been modified — the database must remain untouched
    REQUIRE(Librova::DatabaseRuntime::CSchemaMigrator::ReadUserVersion(databasePath) == 2);

    std::filesystem::remove(databasePath);
}

