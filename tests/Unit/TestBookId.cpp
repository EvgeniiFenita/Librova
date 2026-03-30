#include <catch2/catch_test_macros.hpp>

#include "Domain/BookId.hpp"

TEST_CASE("Book id is valid only for positive values", "[domain][book-id]")
{
    REQUIRE(LibriFlow::Domain::SBookId{1}.IsValid());
    REQUIRE_FALSE(LibriFlow::Domain::SBookId{0}.IsValid());
    REQUIRE_FALSE(LibriFlow::Domain::SBookId{-42}.IsValid());
}
