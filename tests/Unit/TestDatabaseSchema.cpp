#include <catch2/catch_test_macros.hpp>

#include "DatabaseSchema/DatabaseSchema.hpp"

TEST_CASE("Database schema exposes current version and migration steps", "[database-schema]")
{
    const auto& migrations = Librova::DatabaseSchema::CDatabaseSchema::GetMigrationStatements();

    REQUIRE(Librova::DatabaseSchema::CDatabaseSchema::GetCurrentVersion() == 1);
    REQUIRE(migrations.size() == 3);
    REQUIRE(migrations[0] == "PRAGMA foreign_keys = ON;");
    REQUIRE(migrations[1] == "PRAGMA journal_mode = WAL;");
}

TEST_CASE("Database schema script contains required MVP tables", "[database-schema]")
{
    const std::string_view script = Librova::DatabaseSchema::CDatabaseSchema::GetCreateSchemaScript();

    REQUIRE(script.find("CREATE TABLE IF NOT EXISTS books") != std::string_view::npos);
    REQUIRE(script.find("CREATE TABLE IF NOT EXISTS authors") != std::string_view::npos);
    REQUIRE(script.find("CREATE TABLE IF NOT EXISTS book_authors") != std::string_view::npos);
    REQUIRE(script.find("CREATE TABLE IF NOT EXISTS tags") != std::string_view::npos);
    REQUIRE(script.find("CREATE TABLE IF NOT EXISTS book_tags") != std::string_view::npos);
    REQUIRE(script.find("CREATE TABLE IF NOT EXISTS formats") == std::string_view::npos);
}

TEST_CASE("Database schema script contains genre infrastructure tables", "[database-schema]")
{
    const std::string_view script = Librova::DatabaseSchema::CDatabaseSchema::GetCreateSchemaScript();

    REQUIRE(script.find("CREATE TABLE IF NOT EXISTS genres") != std::string_view::npos);
    REQUIRE(script.find("CREATE TABLE IF NOT EXISTS book_genres") != std::string_view::npos);
    REQUIRE(script.find("CREATE INDEX IF NOT EXISTS idx_book_genres_genre_id") != std::string_view::npos);
}

TEST_CASE("Database schema FTS search_index has genres column", "[database-schema]")
{
    const std::string_view script = Librova::DatabaseSchema::CDatabaseSchema::GetCreateSchemaScript();

    // 5-column FTS includes a genres column alongside title, authors, tags, description
    REQUIRE(script.find("search_index USING fts5") != std::string_view::npos);
    REQUIRE(script.find("genres") != std::string_view::npos);
}

TEST_CASE("Database schema script contains required indexes", "[database-schema]")
{
    const std::string_view script = Librova::DatabaseSchema::CDatabaseSchema::GetCreateSchemaScript();

    REQUIRE(script.find("CREATE VIRTUAL TABLE IF NOT EXISTS search_index USING fts5") != std::string_view::npos);
    REQUIRE(script.find("CREATE INDEX IF NOT EXISTS idx_books_language") != std::string_view::npos);
    REQUIRE(script.find("CREATE INDEX IF NOT EXISTS idx_books_normalized_title") != std::string_view::npos);
    REQUIRE(script.find("CREATE INDEX IF NOT EXISTS idx_books_isbn") != std::string_view::npos);
    REQUIRE(script.find("CREATE INDEX IF NOT EXISTS idx_book_authors_author_id") != std::string_view::npos);
    REQUIRE(script.find("CREATE INDEX IF NOT EXISTS idx_book_tags_tag_id") != std::string_view::npos);
}

