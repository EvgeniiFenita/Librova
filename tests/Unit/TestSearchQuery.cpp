#include <catch2/catch_test_macros.hpp>

#include "Domain/SearchQuery.hpp"

TEST_CASE("Search query reports text and structured filters", "[domain][query]")
{
    LibriFlow::Domain::SSearchQuery query;

    REQUIRE_FALSE(query.HasText());
    REQUIRE_FALSE(query.HasStructuredFilters());

    query.TextUtf8 = "strugatsky";
    REQUIRE(query.HasText());
    REQUIRE_FALSE(query.HasStructuredFilters());

    query.AuthorUtf8 = "Arkady Strugatsky";
    REQUIRE(query.HasStructuredFilters());
}

TEST_CASE("Search query keeps MVP defaults for pagination", "[domain][query]")
{
    const LibriFlow::Domain::SSearchQuery query;

    REQUIRE(query.Offset == 0);
    REQUIRE(query.Limit == 50);
}
