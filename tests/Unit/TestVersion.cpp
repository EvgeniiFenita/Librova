#include <catch2/catch_test_macros.hpp>

#include "Core/Version.hpp"

TEST_CASE("Core version is available", "[core][bootstrap]")
{
    REQUIRE(!Librova::Core::CVersion::GetValue().empty());
    REQUIRE(Librova::Core::CVersion::GetValue().find('.') != std::string_view::npos);
}

