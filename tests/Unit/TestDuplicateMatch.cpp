#include <catch2/catch_test_macros.hpp>

#include "Domain/DuplicateMatch.hpp"

TEST_CASE("Duplicate match exposes strict rejection rule", "[domain][duplicate]")
{
    const Librova::Domain::SDuplicateMatch strictMatch{
        .Severity = Librova::Domain::EDuplicateSeverity::Strict,
        .Reason = Librova::Domain::EDuplicateReason::SameHash,
        .ExistingBookId = {10}
    };

    REQUIRE(strictMatch.IsStrictRejection());
    REQUIRE_FALSE(strictMatch.RequiresUserDecision());
}

TEST_CASE("Duplicate match exposes probable duplicate confirmation rule", "[domain][duplicate]")
{
    const Librova::Domain::SDuplicateMatch probableMatch{
        .Severity = Librova::Domain::EDuplicateSeverity::Probable,
        .Reason = Librova::Domain::EDuplicateReason::SameNormalizedTitleAndAuthors,
        .ExistingBookId = {25}
    };

    REQUIRE_FALSE(probableMatch.IsStrictRejection());
    REQUIRE(probableMatch.RequiresUserDecision());
}
