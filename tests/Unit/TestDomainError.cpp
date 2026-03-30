#include <catch2/catch_test_macros.hpp>

#include "Domain/DomainError.hpp"

TEST_CASE("Domain error codes convert to stable transport-friendly strings", "[domain][error]")
{
    REQUIRE(Librova::Domain::ToString(Librova::Domain::EDomainErrorCode::Validation) == "validation");
    REQUIRE(Librova::Domain::ToString(Librova::Domain::EDomainErrorCode::DuplicateDecisionRequired) == "duplicate_decision_required");
    REQUIRE(Librova::Domain::ToString(Librova::Domain::EDomainErrorCode::IntegrityIssue) == "integrity_issue");
}

TEST_CASE("Domain error exposes user-resolvable categories", "[domain][error]")
{
    const Librova::Domain::SDomainError validationError{
        .Code = Librova::Domain::EDomainErrorCode::Validation,
        .Message = "Title is required."
    };

    const Librova::Domain::SDomainError converterUnavailableError{
        .Code = Librova::Domain::EDomainErrorCode::ConverterUnavailable,
        .Message = "Converter is not configured."
    };

    REQUIRE(validationError.IsUserResolvable());
    REQUIRE(converterUnavailableError.IsUserResolvable());
    REQUIRE_FALSE(validationError.IsInfrastructureFailure());
}

TEST_CASE("Domain error exposes infrastructure failure categories", "[domain][error]")
{
    const Librova::Domain::SDomainError storageError{
        .Code = Librova::Domain::EDomainErrorCode::StorageFailure,
        .Message = "Failed to commit staged files."
    };

    REQUIRE(storageError.IsInfrastructureFailure());
    REQUIRE_FALSE(storageError.IsUserResolvable());
}
