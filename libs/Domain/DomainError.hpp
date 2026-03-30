#pragma once

#include <string>
#include <string_view>

namespace LibriFlow::Domain {

enum class EDomainErrorCode
{
    Validation,
    UnsupportedFormat,
    DuplicateRejected,
    DuplicateDecisionRequired,
    ParserFailure,
    ConverterUnavailable,
    ConverterFailed,
    StorageFailure,
    DatabaseFailure,
    Cancellation,
    IntegrityIssue
};

struct SDomainError
{
    EDomainErrorCode Code = EDomainErrorCode::Validation;
    std::string Message;

    [[nodiscard]] bool IsUserResolvable() const noexcept
    {
        return Code == EDomainErrorCode::Validation
            || Code == EDomainErrorCode::DuplicateDecisionRequired
            || Code == EDomainErrorCode::ConverterUnavailable;
    }

    [[nodiscard]] bool IsInfrastructureFailure() const noexcept
    {
        return Code == EDomainErrorCode::ParserFailure
            || Code == EDomainErrorCode::ConverterFailed
            || Code == EDomainErrorCode::StorageFailure
            || Code == EDomainErrorCode::DatabaseFailure
            || Code == EDomainErrorCode::IntegrityIssue;
    }
};

[[nodiscard]] std::string_view ToString(EDomainErrorCode code) noexcept;

} // namespace LibriFlow::Domain
