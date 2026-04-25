#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <Windows.h>

#include "App/LibraryBootstrap.hpp"

#include "TestWorkspace.hpp"

namespace {


void WriteTextFile(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

void PrepareExistingLibraryRoot(const std::filesystem::path& libraryRoot)
{
    std::filesystem::create_directories(libraryRoot / "Database");
    std::filesystem::create_directories(libraryRoot / "Objects");
    std::filesystem::create_directories(libraryRoot / "Logs");
    std::filesystem::create_directories(libraryRoot / "Trash");
    WriteTextFile(libraryRoot / "Database" / "librova.db", "sqlite-placeholder");
}

} // namespace

TEST_CASE("Library bootstrap creates library layout for a new library root", "[application]")
{
    CTestWorkspace sandbox(L"librova-library-bootstrap");
    const auto libraryRoot = sandbox.GetPath() / "Library";

    Librova::Application::CLibraryBootstrap::PrepareLibraryRoot(
        libraryRoot,
        Librova::Application::ELibraryOpenMode::CreateNew);

    REQUIRE(std::filesystem::exists(libraryRoot / "Database"));
    REQUIRE(std::filesystem::exists(libraryRoot / "Objects"));
    REQUIRE(std::filesystem::exists(libraryRoot / "Logs"));
    REQUIRE(std::filesystem::exists(libraryRoot / "Trash"));
    REQUIRE_FALSE(std::filesystem::exists(libraryRoot / "Temp"));
}

TEST_CASE("Library bootstrap opens an existing managed library without requiring Temp", "[application]")
{
    CTestWorkspace sandbox(L"librova-library-bootstrap-existing");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    PrepareExistingLibraryRoot(libraryRoot);

    Librova::Application::CLibraryBootstrap::PrepareLibraryRoot(
        libraryRoot,
        Librova::Application::ELibraryOpenMode::Open);

    REQUIRE(std::filesystem::exists(libraryRoot / "Database" / "librova.db"));
    REQUIRE_FALSE(std::filesystem::exists(libraryRoot / "Temp"));
}

TEST_CASE("Library bootstrap rejects opening a missing managed library", "[application]")
{
    CTestWorkspace sandbox(L"librova-library-bootstrap-open-missing");
    const auto libraryRoot = sandbox.GetPath() / "MissingLibrary";

    REQUIRE_THROWS_WITH(
        Librova::Application::CLibraryBootstrap::PrepareLibraryRoot(
            libraryRoot,
            Librova::Application::ELibraryOpenMode::Open),
        Catch::Matchers::ContainsSubstring("does not exist"));
}

TEST_CASE("Library bootstrap rejects creating a library in a non-empty directory", "[application]")
{
    CTestWorkspace sandbox(L"librova-library-bootstrap-create-non-empty");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    WriteTextFile(libraryRoot / "existing.txt", "occupied");

    REQUIRE_THROWS_WITH(
        Librova::Application::CLibraryBootstrap::PrepareLibraryRoot(
            libraryRoot,
            Librova::Application::ELibraryOpenMode::CreateNew),
        Catch::Matchers::ContainsSubstring("must be empty"));
}
