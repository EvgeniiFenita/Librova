#include <catch2/catch_test_macros.hpp>

#include "Core/Version.hpp"

TEST_CASE("Core version is available", "[core][bootstrap]")
{
    REQUIRE(Librova::Core::CVersion::GetValue() == "0.1.0");
}
