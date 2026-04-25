#include <catch2/catch_test_macros.hpp>

#include "Import/ImportDiagnosticText.hpp"

TEST_CASE("Import diagnostic text joins warnings and terminal error with a stable separator", "[importing]")
{
    REQUIRE(
        Librova::Importing::CImportDiagnosticText::JoinWarningsAndError(
            {"warning one", "warning two"},
            "terminal error")
        == "warning one | warning two | terminal error");
}

TEST_CASE("Import diagnostic text returns warnings without appending empty error text", "[importing]")
{
    REQUIRE(
        Librova::Importing::CImportDiagnosticText::JoinWarningsAndError(
            {"warning one", "", "warning two"},
            "")
        == "warning one | warning two");
}

TEST_CASE("Import diagnostic text returns terminal error when warnings are absent", "[importing]")
{
    REQUIRE(
        Librova::Importing::CImportDiagnosticText::JoinWarningsAndError(
            {},
            "terminal error")
        == "terminal error");
}
