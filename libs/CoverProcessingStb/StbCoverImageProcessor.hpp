#pragma once

#include "Domain/ServiceContracts.hpp"

namespace Librova::CoverProcessingStb {

class CStbCoverImageProcessor final : public Librova::Domain::ICoverImageProcessor
{
public:
    [[nodiscard]] Librova::Domain::SCoverProcessingResult ProcessForManagedStorage(
        const Librova::Domain::SCoverProcessingRequest& request) const override;
};

} // namespace Librova::CoverProcessingStb
