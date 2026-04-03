#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <Windows.h>

#include "CoreHost/LibraryBootstrap.hpp"

namespace {

class CScopedDirectory final
{
public:
    explicit CScopedDirectory(std::filesystem::path path)
        : m_path(std::move(path))
    {
        std::filesystem::remove_all(m_path);
        std::filesystem::create_directories(m_path);
    }

    ~CScopedDirectory()
    {
        std::error_code errorCode;
        std::filesystem::remove_all(m_path, errorCode);
    }

    [[nodiscard]] const std::filesystem::path& GetPath() const noexcept
    {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

void WriteTextFile(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

std::filesystem::path BuildUniqueSandboxPath(const std::wstring_view prefix)
{
    const auto suffix = std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(GetTickCount64());
    return std::filesystem::temp_directory_path() / std::filesystem::path{std::wstring(prefix) + L"." + suffix};
}

void PrepareExistingLibraryRoot(const std::filesystem::path& libraryRoot)
{
    std::filesystem::create_directories(libraryRoot / "Database");
    std::filesystem::create_directories(libraryRoot / "Books");
    std::filesystem::create_directories(libraryRoot / "Covers");
    std::filesystem::create_directories(libraryRoot / "Temp");
    std::filesystem::create_directories(libraryRoot / "Logs");
    std::filesystem::create_directories(libraryRoot / "Trash");
    WriteTextFile(libraryRoot / "Database" / "librova.db", "sqlite-placeholder");
}

} // namespace

TEST_CASE("Library bootstrap creates library layout for a new library root", "[core-host]")
{
    CScopedDirectory sandbox(BuildUniqueSandboxPath(L"librova-library-bootstrap"));
    const auto libraryRoot = sandbox.GetPath() / "Library";

    Librova::CoreHost::CLibraryBootstrap::PrepareLibraryRoot(
        libraryRoot,
        Librova::CoreHost::ELibraryOpenMode::CreateNew);

    REQUIRE(std::filesystem::exists(libraryRoot / "Database"));
    REQUIRE(std::filesystem::exists(libraryRoot / "Books"));
    REQUIRE(std::filesystem::exists(libraryRoot / "Covers"));
    REQUIRE(std::filesystem::exists(libraryRoot / "Temp"));
    REQUIRE(std::filesystem::exists(libraryRoot / "Logs"));
    REQUIRE(std::filesystem::exists(libraryRoot / "Trash"));
}

TEST_CASE("Library bootstrap clears stale temp state for an existing managed library", "[core-host]")
{
    CScopedDirectory sandbox(BuildUniqueSandboxPath(L"librova-library-bootstrap-existing"));
    const auto libraryRoot = sandbox.GetPath() / "Library";
    PrepareExistingLibraryRoot(libraryRoot);
    WriteTextFile(libraryRoot / "Temp" / "stale" / "file.tmp", "stale");

    Librova::CoreHost::CLibraryBootstrap::PrepareLibraryRoot(
        libraryRoot,
        Librova::CoreHost::ELibraryOpenMode::OpenExisting);

    REQUIRE(std::filesystem::exists(libraryRoot / "Database" / "librova.db"));
    REQUIRE(std::filesystem::exists(libraryRoot / "Temp"));
    REQUIRE_FALSE(std::filesystem::exists(libraryRoot / "Temp" / "stale" / "file.tmp"));
}

TEST_CASE("Library bootstrap rejects opening a missing managed library", "[core-host]")
{
    CScopedDirectory sandbox(BuildUniqueSandboxPath(L"librova-library-bootstrap-open-missing"));
    const auto libraryRoot = sandbox.GetPath() / "MissingLibrary";

    REQUIRE_THROWS_WITH(
        Librova::CoreHost::CLibraryBootstrap::PrepareLibraryRoot(
            libraryRoot,
            Librova::CoreHost::ELibraryOpenMode::OpenExisting),
        Catch::Matchers::ContainsSubstring("does not exist"));
}

TEST_CASE("Library bootstrap rejects creating a library in a non-empty directory", "[core-host]")
{
    CScopedDirectory sandbox(BuildUniqueSandboxPath(L"librova-library-bootstrap-create-non-empty"));
    const auto libraryRoot = sandbox.GetPath() / "Library";
    WriteTextFile(libraryRoot / "existing.txt", "occupied");

    REQUIRE_THROWS_WITH(
        Librova::CoreHost::CLibraryBootstrap::PrepareLibraryRoot(
            libraryRoot,
            Librova::CoreHost::ELibraryOpenMode::CreateNew),
        Catch::Matchers::ContainsSubstring("must be empty"));
}
