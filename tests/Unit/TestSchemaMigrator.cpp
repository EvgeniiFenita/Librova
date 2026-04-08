#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>

#include "DatabaseRuntime/SchemaMigrator.hpp"
#include "Sqlite/SqliteConnection.hpp"
#include "Sqlite/SqliteStatement.hpp"

TEST_CASE("Schema migrator applies schema and sets user version", "[database-runtime]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-schema-migrator.db";
    std::filesystem::remove(databasePath);

    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    REQUIRE(Librova::DatabaseRuntime::CSchemaMigrator::ReadUserVersion(databasePath) == 4);

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

    REQUIRE(Librova::DatabaseRuntime::CSchemaMigrator::ReadUserVersion(databasePath) == 4);

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
        Catch::Matchers::ContainsSubstring("9999") && Catch::Matchers::ContainsSubstring("4"));

    // user_version must NOT have been downgraded — the database must remain untouched
    REQUIRE(Librova::DatabaseRuntime::CSchemaMigrator::ReadUserVersion(databasePath) == 9999);

    std::filesystem::remove(databasePath);
}

TEST_CASE("Schema migrator upgrades version 1 databases with normalized titles", "[database-runtime]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-schema-migrator-upgrade-v2.db";
    std::filesystem::remove(databasePath);

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);
        connection.Execute("PRAGMA foreign_keys = ON;");
        connection.Execute("PRAGMA journal_mode = WAL;");
        connection.Execute(R"sql(
            CREATE TABLE books (
                id INTEGER PRIMARY KEY,
                title TEXT NOT NULL,
                language TEXT NOT NULL,
                series TEXT,
                series_index REAL,
                publisher TEXT,
                year INTEGER,
                isbn TEXT,
                description TEXT,
                identifier TEXT,
                preferred_format TEXT NOT NULL,
                managed_path TEXT NOT NULL,
                cover_path TEXT,
                file_size_bytes INTEGER NOT NULL,
                sha256_hex TEXT NOT NULL,
                added_at_utc TEXT NOT NULL
            );
        )sql");
        connection.Execute("INSERT INTO books (id, title, language, preferred_format, managed_path, file_size_bytes, sha256_hex, added_at_utc) "
                           "VALUES (1, 'Ежик   в тумане', 'ru', 'epub', 'Books/0000000001/book.epub', 100, 'hash-v1', '2026-03-30T00:00:00Z');");
        connection.Execute("PRAGMA user_version = 1;");
    }

    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    REQUIRE(Librova::DatabaseRuntime::CSchemaMigrator::ReadUserVersion(databasePath) == 4);

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);
        Librova::Sqlite::CSqliteStatement titleStatement(
            connection.GetNativeHandle(),
            "SELECT normalized_title FROM books WHERE id = 1;");
        REQUIRE(titleStatement.Step());
        REQUIRE(titleStatement.GetColumnText(0) == "ежик в тумане");

        Librova::Sqlite::CSqliteStatement indexStatement(
            connection.GetNativeHandle(),
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'index' AND name = 'idx_books_normalized_title';");
        REQUIRE(indexStatement.Step());
        REQUIRE(indexStatement.GetColumnInt(0) == 1);

        Librova::Sqlite::CSqliteStatement storageEncodingStatement(
            connection.GetNativeHandle(),
            "SELECT storage_encoding FROM books WHERE id = 1;");
        REQUIRE(storageEncodingStatement.Step());
        REQUIRE(storageEncodingStatement.GetColumnText(0) == "plain");
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Schema migrator drops obsolete formats table from version 3 databases", "[database-runtime]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-schema-migrator-drop-formats.db";
    std::filesystem::remove(databasePath);

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);
        connection.Execute("PRAGMA foreign_keys = ON;");
        connection.Execute("PRAGMA journal_mode = WAL;");
        connection.Execute(R"sql(
            CREATE TABLE books (
                id INTEGER PRIMARY KEY,
                title TEXT NOT NULL,
                normalized_title TEXT NOT NULL,
                language TEXT NOT NULL,
                series TEXT,
                series_index REAL,
                publisher TEXT,
                year INTEGER,
                isbn TEXT,
                description TEXT,
                identifier TEXT,
                preferred_format TEXT NOT NULL,
                storage_encoding TEXT NOT NULL DEFAULT 'plain',
                managed_path TEXT NOT NULL,
                cover_path TEXT,
                file_size_bytes INTEGER NOT NULL,
                sha256_hex TEXT NOT NULL,
                added_at_utc TEXT NOT NULL
            );
        )sql");
        connection.Execute(R"sql(
            CREATE TABLE formats (
                book_id INTEGER PRIMARY KEY,
                format TEXT NOT NULL,
                storage_encoding TEXT NOT NULL DEFAULT 'plain',
                managed_path TEXT NOT NULL,
                file_size_bytes INTEGER NOT NULL,
                sha256_hex TEXT NOT NULL,
                FOREIGN KEY (book_id) REFERENCES books(id) ON DELETE CASCADE
            );
        )sql");
        connection.Execute("INSERT INTO books (id, title, normalized_title, language, preferred_format, storage_encoding, managed_path, file_size_bytes, sha256_hex, added_at_utc) "
                           "VALUES (1, 'Roadside Picnic', 'roadside picnic', 'en', 'epub', 'plain', 'Books/0000000001/book.epub', 100, 'hash-v3', '2026-03-30T00:00:00Z');");
        connection.Execute("INSERT INTO formats (book_id, format, storage_encoding, managed_path, file_size_bytes, sha256_hex) "
                           "VALUES (1, 'epub', 'plain', 'Books/0000000001/book.epub', 100, 'hash-v3');");
        connection.Execute("PRAGMA user_version = 3;");
    }

    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    REQUIRE(Librova::DatabaseRuntime::CSchemaMigrator::ReadUserVersion(databasePath) == 4);

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);

        Librova::Sqlite::CSqliteStatement booksStatement(
            connection.GetNativeHandle(),
            "SELECT preferred_format, storage_encoding, managed_path, sha256_hex FROM books WHERE id = 1;");
        REQUIRE(booksStatement.Step());
        REQUIRE(booksStatement.GetColumnText(0) == "epub");
        REQUIRE(booksStatement.GetColumnText(1) == "plain");
        REQUIRE(booksStatement.GetColumnText(2) == "Books/0000000001/book.epub");
        REQUIRE(booksStatement.GetColumnText(3) == "hash-v3");

        Librova::Sqlite::CSqliteStatement formatsStatement(
            connection.GetNativeHandle(),
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = 'formats';");
        REQUIRE(formatsStatement.Step());
        REQUIRE(formatsStatement.GetColumnInt(0) == 0);
    }

    std::filesystem::remove(databasePath);
}
