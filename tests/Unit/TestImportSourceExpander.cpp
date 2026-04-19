#include <catch2/catch_test_macros.hpp>
#include "TestWorkspace.hpp"

#include <filesystem>
#include <fstream>

#include "Import/ImportSourceExpander.hpp"

TEST_CASE("Import source expander expands directories recursively and ignores unsupported files", "[import-source-expander]")
{
    const auto sandbox = MakeUniqueTestPath(L"librova-import-source-expander-directory");
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "nested");
    std::ofstream(sandbox / "nested" / "book.fb2").put('a');
    std::ofstream(sandbox / "nested" / "book.epub").put('b');
    std::ofstream(sandbox / "nested" / "notes.txt").put('c');

    const auto expanded = Librova::ImportSourceExpander::CImportSourceExpander::Expand({sandbox});

    REQUIRE(expanded.Candidates.size() == 2);
    REQUIRE(expanded.Warnings.empty());

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Import source expander reports unsupported standalone files as warnings", "[import-source-expander]")
{
    const auto sandbox = MakeUniqueTestPath(L"librova-import-source-expander-unsupported");
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);
    const auto unsupportedPath = sandbox / "notes.txt";
    std::ofstream(unsupportedPath).put('x');

    const auto expanded = Librova::ImportSourceExpander::CImportSourceExpander::Expand({unsupportedPath});

    REQUIRE(expanded.Candidates.empty());
    REQUIRE(expanded.Warnings.size() == 1);
    REQUIRE(expanded.Warnings.front().find("Only .fb2, .epub, and .zip are supported.") != std::string::npos);

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Import source expander reports missing sources as warnings", "[import-source-expander]")
{
    const auto missingPath = MakeUniqueTestPath(L"librova-import-source-expander-missing.fb2");
    std::filesystem::remove(missingPath);

    const auto expanded = Librova::ImportSourceExpander::CImportSourceExpander::Expand({missingPath});

    REQUIRE(expanded.Candidates.empty());
    REQUIRE(expanded.Warnings.size() == 1);
    REQUIRE(expanded.Warnings.front().find("does not exist") != std::string::npos);
}

