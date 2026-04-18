#pragma once

#include <filesystem>
#include <mutex>

#include "Domain/ServiceContracts.hpp"

namespace Librova::ManagedStorage {

class CManagedFileStorage final : public Librova::Domain::IManagedStorage
{
public:
    CManagedFileStorage(
        std::filesystem::path libraryRoot,
        std::filesystem::path stagingRoot);

    [[nodiscard]] Librova::Domain::SPreparedStorage PrepareImport(const Librova::Domain::SStoragePlan& plan) override;
    void CommitImport(const Librova::Domain::SPreparedStorage& preparedStorage) override;
    void RollbackImport(const Librova::Domain::SPreparedStorage& preparedStorage) noexcept override;

private:
    std::filesystem::path m_libraryRoot;
    std::filesystem::path m_stagingRoot;
    mutable std::once_flag m_rootDirsEnsuredOnce;
};

} // namespace Librova::ManagedStorage
