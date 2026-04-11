#pragma once

#include <filesystem>
#include <map>
#include <stop_token>
#include <string>
#include <vector>

#include "Domain/ServiceContracts.hpp"
#include "ZipImporting/ZipImportCoordinator.hpp"

namespace Librova::Application {

struct SPreparedImportWorkload
{
    std::size_t TotalEntries = 0;
    std::map<std::filesystem::path, std::size_t> PlannedEntriesBySource;
    std::vector<std::string> Warnings;
};

class CImportWorkloadPlanner final
{
public:
    explicit CImportWorkloadPlanner(const Librova::ZipImporting::CZipImportCoordinator& zipImportCoordinator);

    [[nodiscard]] SPreparedImportWorkload Prepare(
        const std::vector<std::filesystem::path>& candidates,
        const Librova::Domain::IProgressSink& progressSink,
        std::stop_token stopToken) const;

private:
    const Librova::ZipImporting::CZipImportCoordinator& m_zipImportCoordinator;
};

} // namespace Librova::Application
