#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <atomic>
#include <filesystem>
#include <system_error>
#include <thread>

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

        connection.Execute("INSERT INTO books (id, title, normalized_title, language, preferred_format, managed_path, file_size_bytes, sha256_hex, added_at_utc) "
                           "VALUES (1, 'Roadside Picnic', 'roadside picnic', 'ru', 'epub', 'Books/0000000001/book.epub', 123, 'abc', '2026-03-30T12:00:00Z');");

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

TEST_CASE("Sqlite connection configures busy timeout for each new connection", "[sqlite]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-sqlite-busy-timeout.db";
    std::filesystem::remove(databasePath);

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);
        Librova::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), "PRAGMA busy_timeout;");

        REQUIRE(statement.Step());
        REQUIRE(statement.GetColumnInt(0) == 5000);
        REQUIRE_FALSE(statement.Step());
    }

    std::filesystem::remove(databasePath);
}

TEST_CASE("Sqlite connection opens a database under a Unicode path", "[sqlite]")
{
    const std::filesystem::path sandboxPath =
        std::filesystem::temp_directory_path() / std::filesystem::path{u8"librova-тест-sqlite"};
    const std::filesystem::path databasePath = sandboxPath / std::filesystem::path{u8"данные.db"};

    std::filesystem::remove_all(sandboxPath);
    std::filesystem::create_directories(sandboxPath);

    {
        Librova::Sqlite::CSqliteConnection connection(databasePath);

        for (const std::string_view statement : Librova::DatabaseSchema::CDatabaseSchema::GetMigrationStatements())
        {
            connection.Execute(statement);
        }

        connection.Execute("INSERT INTO books (id, title, normalized_title, language, preferred_format, managed_path, file_size_bytes, sha256_hex, added_at_utc) "
                           "VALUES (1, 'Metro 2033', 'metro 2033', 'ru', 'epub', 'Books/0000000001/book.epub', 123, 'hash', '2026-03-30T12:00:00Z');");

        Librova::Sqlite::CSqliteStatement statement(connection.GetNativeHandle(), "SELECT title FROM books WHERE id = 1;");

        REQUIRE(statement.Step());
        REQUIRE(statement.GetColumnText(0) == "Metro 2033");
        REQUIRE_FALSE(statement.Step());
    }

    std::error_code errorCode;
    std::filesystem::remove_all(sandboxPath, errorCode);
}

TEST_CASE("Sqlite connection waits for an overlapping write transaction instead of failing immediately", "[sqlite]")
{
    const std::filesystem::path databasePath = std::filesystem::temp_directory_path() / "librova-sqlite-write-contention.db";
    std::filesystem::remove(databasePath);

    {
        Librova::Sqlite::CSqliteConnection setupConnection(databasePath);
        setupConnection.Execute("CREATE TABLE contention_test(id INTEGER PRIMARY KEY, value TEXT NOT NULL);");
    }

    {
        Librova::Sqlite::CSqliteConnection firstConnection(databasePath);
        firstConnection.Execute("BEGIN IMMEDIATE;");
        firstConnection.Execute("INSERT INTO contention_test(id, value) VALUES (1, 'first');");

        std::atomic<bool> secondInsertSucceeded = false;
        std::atomic<bool> secondInsertStarted = false;
        std::exception_ptr workerFailure;

        std::jthread worker([&] {
            try
            {
                secondInsertStarted.store(true);

                Librova::Sqlite::CSqliteConnection secondConnection(databasePath);
                secondConnection.Execute("BEGIN IMMEDIATE;");
                secondConnection.Execute("INSERT INTO contention_test(id, value) VALUES (2, 'second');");
                secondConnection.Execute("COMMIT;");
                secondInsertSucceeded.store(true);
            }
            catch (...)
            {
                workerFailure = std::current_exception();
            }
        });

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!secondInsertStarted.load() && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        REQUIRE(secondInsertStarted.load());
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        REQUIRE_FALSE(secondInsertSucceeded.load());

        firstConnection.Execute("COMMIT;");
        worker.join();

        if (workerFailure != nullptr)
        {
            std::rethrow_exception(workerFailure);
        }

        REQUIRE(secondInsertSucceeded.load());

        {
            Librova::Sqlite::CSqliteConnection verifyConnection(databasePath);
            Librova::Sqlite::CSqliteStatement statement(
                verifyConnection.GetNativeHandle(),
                "SELECT COUNT(*) FROM contention_test;");

            REQUIRE(statement.Step());
            REQUIRE(statement.GetColumnInt(0) == 2);
            REQUIRE_FALSE(statement.Step());
        }
    }

    std::filesystem::remove(databasePath);
}
