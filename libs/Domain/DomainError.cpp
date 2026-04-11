#include "Domain/DomainError.hpp"

namespace Librova::Domain {

std::string_view ToString(const EDomainErrorCode code) noexcept
{
    switch (code)
    {
    case EDomainErrorCode::Validation:
        return "validation";
    case EDomainErrorCode::UnsupportedFormat:
        return "unsupported_format";
    case EDomainErrorCode::DuplicateRejected:
        return "duplicate_rejected";
    case EDomainErrorCode::DuplicateDecisionRequired:
        return "duplicate_decision_required";
    case EDomainErrorCode::ParserFailure:
        return "parser_failure";
    case EDomainErrorCode::ConverterUnavailable:
        return "converter_unavailable";
    case EDomainErrorCode::ConverterFailed:
        return "converter_failed";
    case EDomainErrorCode::StorageFailure:
        return "storage_failure";
    case EDomainErrorCode::DatabaseFailure:
        return "database_failure";
    case EDomainErrorCode::Cancellation:
        return "cancellation";
    case EDomainErrorCode::IntegrityIssue:
        return "integrity_issue";
    case EDomainErrorCode::NotFound:
        return "not_found";
    }

    return "unknown";
}

} // namespace Librova::Domain
