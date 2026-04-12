#pragma once

#include "Domain/BookId.hpp"

namespace Librova::Domain {

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
    std::string ExistingTitle;
    std::string ExistingAuthors;

    [[nodiscard]] bool IsStrictRejection() const noexcept
    {
        return Severity == EDuplicateSeverity::Strict;
    }

    [[nodiscard]] bool IsProbable() const noexcept
    {
        return Severity == EDuplicateSeverity::Probable;
    }
};

} // namespace Librova::Domain
