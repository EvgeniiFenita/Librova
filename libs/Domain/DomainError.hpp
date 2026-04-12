#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace Librova::Domain {

enum class EDomainErrorCode
{
    Validation,
    UnsupportedFormat,
    DuplicateRejected,
    ParserFailure,
    ConverterUnavailable,
    ConverterFailed,
    StorageFailure,
    DatabaseFailure,
    Cancellation,
    IntegrityIssue,
    NotFound
};

struct SDomainError
{
    EDomainErrorCode Code = EDomainErrorCode::Validation;
    std::string Message;

    [[nodiscard]] bool IsUserResolvable() const noexcept
    {
        return Code == EDomainErrorCode::Validation
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

class CDomainException : public std::runtime_error
{
public:
    CDomainException(EDomainErrorCode code, std::string message)
        : std::runtime_error(message)
        , m_code(code)
    {
    }

    [[nodiscard]] EDomainErrorCode Code() const noexcept
    {
        return m_code;
    }

private:
    EDomainErrorCode m_code;
};

[[nodiscard]] std::string_view ToString(EDomainErrorCode code) noexcept;

} // namespace Librova::Domain
