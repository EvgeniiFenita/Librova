#include <catch2/catch_test_macros.hpp>

#include "Domain/SearchQuery.hpp"

TEST_CASE("Search query reports text and structured filters", "[domain][query]")
{
    Librova::Domain::SSearchQuery query;

    REQUIRE_FALSE(query.HasText());
    REQUIRE_FALSE(query.HasStructuredFilters());

    query.TextUtf8 = "strugatsky";
    REQUIRE(query.HasText());
    REQUIRE_FALSE(query.HasStructuredFilters());

    query.AuthorUtf8 = "Arkady Strugatsky";
    REQUIRE(query.HasStructuredFilters());
}

TEST_CASE("Search query reports Languages as structured filter", "[domain][query]")
{
    Librova::Domain::SSearchQuery query;
    REQUIRE_FALSE(query.HasStructuredFilters());

    query.Languages = {"en", "ru"};
    REQUIRE(query.HasStructuredFilters());
}

TEST_CASE("Search query reports GenresUtf8 as structured filter", "[domain][query]")
{
    Librova::Domain::SSearchQuery query;
    REQUIRE_FALSE(query.HasStructuredFilters());

    query.GenresUtf8 = {"classic"};
    REQUIRE(query.HasStructuredFilters());
}

TEST_CASE("Search query keeps MVP defaults for pagination", "[domain][query]")
{
    const Librova::Domain::SSearchQuery query;

    REQUIRE(query.Offset == 0);
    REQUIRE(query.Limit == 50);
}
