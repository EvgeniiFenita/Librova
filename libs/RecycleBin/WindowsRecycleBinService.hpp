#pragma once

#include <filesystem>
#include <vector>

#include "Domain/ServiceContracts.hpp"

namespace Librova::RecycleBin {

class CWindowsRecycleBinService final : public Librova::Domain::IRecycleBinService
{
public:
    void MoveToRecycleBin(const std::vector<std::filesystem::path>& paths) override;
};

} // namespace Librova::RecycleBin
