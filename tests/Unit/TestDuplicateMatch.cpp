#include <catch2/catch_test_macros.hpp>

#include "Domain/DuplicateMatch.hpp"

TEST_CASE("Duplicate match exposes strict rejection rule", "[domain][duplicate]")
{
    const LibriFlow::Domain::SDuplicateMatch strictMatch{
        .Severity = LibriFlow::Domain::EDuplicateSeverity::Strict,
        .Reason = LibriFlow::Domain::EDuplicateReason::SameHash,
        .ExistingBookId = {10}
    };

    REQUIRE(strictMatch.IsStrictRejection());
    REQUIRE_FALSE(strictMatch.RequiresUserDecision());
}

TEST_CASE("Duplicate match exposes probable duplicate confirmation rule", "[domain][duplicate]")
{
    const LibriFlow::Domain::SDuplicateMatch probableMatch{
        .Severity = LibriFlow::Domain::EDuplicateSeverity::Probable,
        .Reason = LibriFlow::Domain::EDuplicateReason::SameNormalizedTitleAndAuthors,
        .ExistingBookId = {25}
    };

    REQUIRE_FALSE(probableMatch.IsStrictRejection());
    REQUIRE(probableMatch.RequiresUserDecision());
}
