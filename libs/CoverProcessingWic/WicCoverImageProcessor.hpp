#pragma once

#include "Domain/ServiceContracts.hpp"

namespace Librova::CoverProcessingWic {

class CWicCoverImageProcessor final : public Librova::Domain::ICoverImageProcessor
{
public:
    [[nodiscard]] Librova::Domain::SCoverProcessingResult ProcessForManagedStorage(
        const Librova::Domain::SCoverProcessingRequest& request) const override;
};

} // namespace Librova::CoverProcessingWic
