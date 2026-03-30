#include <catch2/catch_test_macros.hpp>

#include "Domain/DomainError.hpp"

TEST_CASE("Domain error codes convert to stable transport-friendly strings", "[domain][error]")
{
    REQUIRE(LibriFlow::Domain::ToString(LibriFlow::Domain::EDomainErrorCode::Validation) == "validation");
    REQUIRE(LibriFlow::Domain::ToString(LibriFlow::Domain::EDomainErrorCode::DuplicateDecisionRequired) == "duplicate_decision_required");
    REQUIRE(LibriFlow::Domain::ToString(LibriFlow::Domain::EDomainErrorCode::IntegrityIssue) == "integrity_issue");
}

TEST_CASE("Domain error exposes user-resolvable categories", "[domain][error]")
{
    const LibriFlow::Domain::SDomainError validationError{
        .Code = LibriFlow::Domain::EDomainErrorCode::Validation,
        .Message = "Title is required."
    };

    const LibriFlow::Domain::SDomainError converterUnavailableError{
        .Code = LibriFlow::Domain::EDomainErrorCode::ConverterUnavailable,
        .Message = "Converter is not configured."
    };

    REQUIRE(validationError.IsUserResolvable());
    REQUIRE(converterUnavailableError.IsUserResolvable());
    REQUIRE_FALSE(validationError.IsInfrastructureFailure());
}

TEST_CASE("Domain error exposes infrastructure failure categories", "[domain][error]")
{
    const LibriFlow::Domain::SDomainError storageError{
        .Code = LibriFlow::Domain::EDomainErrorCode::StorageFailure,
        .Message = "Failed to commit staged files."
    };

    REQUIRE(storageError.IsInfrastructureFailure());
    REQUIRE_FALSE(storageError.IsUserResolvable());
}
