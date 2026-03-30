#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "DatabaseSchema/DatabaseSchema.hpp"
#include "Sqlite/SqliteConnection.hpp"
#include "Sqlite/SqliteStatement.hpp"

TEST_CASE("Sqlite connection can apply schema migrations to a temporary database", "[sqlite]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-sqlite-smoke.db";
    std::filesystem::remove(databasePath);

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);

        for (const std::string_view statement : Librova::DatabaseSchema::CDatabaseSchema::GetMigrationStatements())
        {
            connection.Execute(statement);
        }

        connection.Execute("INSERT INTO books (id, title, language, preferred_format, managed_path, file_size_bytes, sha256_hex, added_at_utc) "
                           "VALUES (1, 'Roadside Picnic', 'ru', 'epub', 'Books/0000000001/book.epub', 123, 'abc', '2026-03-30T12:00:00Z');");

        Librova::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), "SELECT COUNT(*) FROM books;");

        REQUIRE(statement.Step());
        REQUIRE(statement.GetColumnInt(0) == 1);
        REQUIRE_FALSE(statement.Step());
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite connection enables foreign key enforcement for each new connection", "[sqlite]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-sqlite-foreign-keys.db";
    std::filesystem::remove(databasePath);

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);
        Librova::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), "PRAGMA foreign_keys;");

        REQUIRE(statement.Step());
        REQUIRE(statement.GetColumnInt(0) == 1);
        REQUIRE_FALSE(statement.Step());
    }

    std::filesystem::remove(databasePath);
}
