#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdint>
#include <filesystem>

#include "Database/SchemaMigrator.hpp"
#include "Database/SqliteConnection.hpp"
#include "Database/SqliteStatement.hpp"
#include "TestWorkspace.hpp"

namespace {

void CreateVersion1SchemaWithBook(const std::filesystem::path& databasePath)
{
    Librova::Sqlite::CSqliteConnection connection(databasePath);
    connection.Execute(
        R"sql(
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

CREATE TABLE book_id_sequence (
    singleton INTEGER PRIMARY KEY CHECK(singleton = 1),
    next_id INTEGER NOT NULL
);

CREATE TABLE authors (
    id INTEGER PRIMARY KEY,
    normalized_name TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL
);

CREATE TABLE book_authors (
    book_id INTEGER NOT NULL,
    author_id INTEGER NOT NULL,
    author_order INTEGER NOT NULL,
    PRIMARY KEY (book_id, author_id),
    FOREIGN KEY (book_id) REFERENCES books(id) ON DELETE CASCADE,
    FOREIGN KEY (author_id) REFERENCES authors(id) ON DELETE CASCADE
);

CREATE TABLE tags (
    id INTEGER PRIMARY KEY,
    normalized_name TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL
);

CREATE TABLE book_tags (
    book_id INTEGER NOT NULL,
    tag_id INTEGER NOT NULL,
    PRIMARY KEY (book_id, tag_id),
    FOREIGN KEY (book_id) REFERENCES books(id) ON DELETE CASCADE,
    FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
);

CREATE TABLE genres (
    id INTEGER PRIMARY KEY,
    normalized_name TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL
);

CREATE TABLE book_genres (
    book_id INTEGER NOT NULL,
    genre_id INTEGER NOT NULL,
    source_type TEXT NOT NULL,
    PRIMARY KEY (book_id, genre_id),
    FOREIGN KEY (book_id) REFERENCES books(id) ON DELETE CASCADE,
    FOREIGN KEY (genre_id) REFERENCES genres(id) ON DELETE CASCADE
);

CREATE VIRTUAL TABLE search_index USING fts5(
    title,
    authors,
    tags,
    genres,
    description,
    content=''
);

CREATE INDEX idx_books_title ON books(title);
CREATE INDEX idx_books_normalized_title ON books(normalized_title);
CREATE INDEX idx_books_language ON books(language);
CREATE INDEX idx_books_series ON books(series);
CREATE INDEX idx_books_year ON books(year);
CREATE INDEX idx_books_added_at_utc ON books(added_at_utc);
CREATE INDEX idx_books_sha256_hex ON books(sha256_hex);
CREATE INDEX idx_books_isbn ON books(isbn);
CREATE INDEX idx_book_authors_author_id ON book_authors(author_id);
CREATE INDEX idx_book_tags_tag_id ON book_tags(tag_id);
CREATE INDEX idx_book_genres_genre_id ON book_genres(genre_id);

INSERT INTO books (
    id,
    title,
    normalized_title,
    language,
    preferred_format,
    storage_encoding,
    managed_path,
    file_size_bytes,
    sha256_hex,
    added_at_utc
) VALUES (
    7,
    'Alpha',
    'alpha',
    'en',
    'epub',
    'plain',
    'Objects/aa/aa/0000000007.book.epub',
    1024,
    'schema-v1-book',
    '2026-04-23T00:00:00Z'
);

PRAGMA user_version = 1;
)sql");
}

} // namespace

TEST_CASE("Schema migrator applies schema and sets user version", "[database-runtime]")
{
    const auto databasePath = MakeUniqueTestPath(L"librova-schema-migrator.db");
    std::filesystem::remove(databasePath);

    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    REQUIRE(Librova::DatabaseRuntime::CSchemaMigrator::ReadUserVersion(databasePath) == 2);

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);
        Librova::Sqlite::CSqliteStatement statement(
            connection.GetNativeHandle(),
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = 'books';");

        REQUIRE(statement.Step());
        REQUIRE(statement.GetColumnInt(0) == 1);
    }

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);
        Librova::Sqlite::CSqliteStatement statement(
            connection.GetNativeHandle(),
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = 'collections';");

        REQUIRE(statement.Step());
        REQUIRE(statement.GetColumnInt(0) == 1);
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Schema migrator is idempotent for existing database", "[database-runtime]")
{
    const auto databasePath = MakeUniqueTestPath(L"librova-schema-migrator-idempotent.db");
    std::filesystem::remove(databasePath);

    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);
    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    REQUIRE(Librova::DatabaseRuntime::CSchemaMigrator::ReadUserVersion(databasePath) == 2);

    std::filesystem::remove(databasePath);
}

TEST_CASE("Schema migrator rejects database with a schema version newer than the current binary supports", "[database-runtime]")
{
    const auto databasePath = MakeUniqueTestPath(L"librova-schema-migrator-future.db");
    std::filesystem::remove(databasePath);

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);
        connection.Execute("PRAGMA user_version = 9999;");
    }

    REQUIRE_THROWS_WITH(
        Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath),
        Catch::Matchers::ContainsSubstring("9999") && Catch::Matchers::ContainsSubstring("2"));

    // user_version must NOT have been downgraded — the database must remain untouched
    REQUIRE(Librova::DatabaseRuntime::CSchemaMigrator::ReadUserVersion(databasePath) == 9999);

    std::filesystem::remove(databasePath);
}

TEST_CASE("Schema migrator upgrades schema version 1 database to version 2", "[database-runtime]")
{
    const auto databasePath = MakeUniqueTestPath(L"librova-schema-migrator-v1-upgrade.db");
    std::filesystem::remove(databasePath);

    CreateVersion1SchemaWithBook(databasePath);

    Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath);

    REQUIRE(Librova::DatabaseRuntime::CSchemaMigrator::ReadUserVersion(databasePath) == 2);

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);
        Librova::Sqlite::CSqliteStatement statement(
            connection.GetNativeHandle(),
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = 'collections';");

        REQUIRE(statement.Step());
        REQUIRE(statement.GetColumnInt(0) == 1);
    }

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);
        connection.Execute(
            "INSERT INTO collections (id, name, normalized_name, icon_key, kind, is_deletable, created_at_utc) "
            "VALUES (3, 'Favorites', 'favorites', 'star', 'user', 1, '2026-04-23T00:00:00Z');");
        connection.Execute("INSERT INTO book_collections (book_id, collection_id) VALUES (7, 3);");

        Librova::Sqlite::CSqliteStatement statement(
            connection.GetNativeHandle(),
            "SELECT COUNT(*) FROM books WHERE id = 7;");

        REQUIRE(statement.Step());
        REQUIRE(statement.GetColumnInt(0) == 1);
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Schema migrator rejects database with incompatible newer schema version", "[database-runtime]")
{
    const auto databasePath = MakeUniqueTestPath(L"librova-schema-migrator-old.db");
    std::filesystem::remove(databasePath);

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);
        connection.Execute("PRAGMA user_version = 3;");
    }

    REQUIRE_THROWS_WITH(
        Librova::DatabaseRuntime::CSchemaMigrator::Migrate(databasePath),
        Catch::Matchers::ContainsSubstring("3") && Catch::Matchers::ContainsSubstring("2"));

    REQUIRE(Librova::DatabaseRuntime::CSchemaMigrator::ReadUserVersion(databasePath) == 3);

    std::filesystem::remove(databasePath);
}
