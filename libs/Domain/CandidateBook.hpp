#pragma once

#include <optional>
#include <string>

#include "Domain/Book.hpp"

namespace LibriFlow::Domain {

struct SCandidateBook
{
    SBookMetadata Metadata;
    EBookFormat Format = EBookFormat::Epub;
    std::optional<std::string> Sha256Hex;

    [[nodiscard]] bool HasHash() const noexcept
    {
        return Sha256Hex.has_value() && !Sha256Hex->empty();
    }

    [[nodiscard]] bool HasIsbn() const noexcept
    {
        return Metadata.Isbn.has_value() && !Metadata.Isbn->empty();
    }
};

} // namespace LibriFlow::Domain
