#pragma once

#include "Domain/BookId.hpp"

namespace LibriFlow::Domain {

enum class EDuplicateSeverity
{
    None,
    Probable,
    Strict
};

enum class EDuplicateReason
{
    SameHash,
    SameIsbn,
    SameNormalizedTitleAndAuthors
};

struct SDuplicateMatch
{
    EDuplicateSeverity Severity = EDuplicateSeverity::None;
    EDuplicateReason Reason = EDuplicateReason::SameHash;
    SBookId ExistingBookId;

    [[nodiscard]] bool IsStrictRejection() const noexcept
    {
        return Severity == EDuplicateSeverity::Strict;
    }

    [[nodiscard]] bool RequiresUserDecision() const noexcept
    {
        return Severity == EDuplicateSeverity::Probable;
    }
};

} // namespace LibriFlow::Domain
