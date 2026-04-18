#include "Database/DatabaseSchema.hpp"

namespace Librova::DatabaseSchema {
namespace {

constexpr std::string_view GCreateSchemaScript = R"sql(
CREATE TABLE IF NOT EXISTS books (
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

CREATE TABLE IF NOT EXISTS book_id_sequence (
    singleton INTEGER PRIMARY KEY CHECK(singleton = 1),
    next_id INTEGER NOT NULL
);

INSERT INTO book_id_sequence(singleton, next_id)
VALUES (1, COALESCE((SELECT MAX(id) + 1 FROM books), 1))
ON CONFLICT(singleton) DO UPDATE SET
    next_id = CASE
        WHEN excluded.next_id > next_id THEN excluded.next_id
        ELSE next_id
    END;

CREATE TABLE IF NOT EXISTS authors (
    id INTEGER PRIMARY KEY,
    normalized_name TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS book_authors (
    book_id INTEGER NOT NULL,
    author_id INTEGER NOT NULL,
    author_order INTEGER NOT NULL,
    PRIMARY KEY (book_id, author_id),
    FOREIGN KEY (book_id) REFERENCES books(id) ON DELETE CASCADE,
    FOREIGN KEY (author_id) REFERENCES authors(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS tags (
    id INTEGER PRIMARY KEY,
    normalized_name TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS book_tags (
    book_id INTEGER NOT NULL,
    tag_id INTEGER NOT NULL,
    PRIMARY KEY (book_id, tag_id),
    FOREIGN KEY (book_id) REFERENCES books(id) ON DELETE CASCADE,
    FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS genres (
    id INTEGER PRIMARY KEY,
    normalized_name TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS book_genres (
    book_id INTEGER NOT NULL,
    genre_id INTEGER NOT NULL,
    source_type TEXT NOT NULL,
    PRIMARY KEY (book_id, genre_id),
    FOREIGN KEY (book_id) REFERENCES books(id) ON DELETE CASCADE,
    FOREIGN KEY (genre_id) REFERENCES genres(id) ON DELETE CASCADE
);

CREATE VIRTUAL TABLE IF NOT EXISTS search_index USING fts5(
    title,
    authors,
    tags,
    genres,
    description,
    content=''
);

CREATE INDEX IF NOT EXISTS idx_books_title ON books(title);
CREATE INDEX IF NOT EXISTS idx_books_normalized_title ON books(normalized_title);
CREATE INDEX IF NOT EXISTS idx_books_language ON books(language);
CREATE INDEX IF NOT EXISTS idx_books_series ON books(series);
CREATE INDEX IF NOT EXISTS idx_books_year ON books(year);
CREATE INDEX IF NOT EXISTS idx_books_added_at_utc ON books(added_at_utc);
CREATE INDEX IF NOT EXISTS idx_books_sha256_hex ON books(sha256_hex);
CREATE INDEX IF NOT EXISTS idx_books_isbn ON books(isbn);
CREATE INDEX IF NOT EXISTS idx_book_authors_author_id ON book_authors(author_id);
CREATE INDEX IF NOT EXISTS idx_book_tags_tag_id ON book_tags(tag_id);
CREATE INDEX IF NOT EXISTS idx_book_genres_genre_id ON book_genres(genre_id);
)sql";

const std::vector<std::string_view> GMigrationStatements{
    "PRAGMA foreign_keys = ON;",
    "PRAGMA journal_mode = WAL;",
    GCreateSchemaScript
};

} // namespace

int CDatabaseSchema::GetCurrentVersion() noexcept
{
    return 1;
}

const std::vector<std::string_view>& CDatabaseSchema::GetMigrationStatements()
{
    return GMigrationStatements;
}

std::string_view CDatabaseSchema::GetCreateSchemaScript() noexcept
{
    return GCreateSchemaScript;
}

} // namespace Librova::DatabaseSchema
