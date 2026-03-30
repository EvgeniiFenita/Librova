#pragma once

#include <filesystem>

#include "Domain/ServiceContracts.hpp"

namespace LibriFlow::ManagedStorage {

class CManagedFileStorage final : public LibriFlow::Domain::IManagedStorage
{
public:
    explicit CManagedFileStorage(std::filesystem::path libraryRoot);

    [[nodiscard]] LibriFlow::Domain::SPreparedStorage PrepareImport(const LibriFlow::Domain::SStoragePlan& plan) override;
    void CommitImport(const LibriFlow::Domain::SPreparedStorage& preparedStorage) override;
    void RollbackImport(const LibriFlow::Domain::SPreparedStorage& preparedStorage) noexcept override;

private:
    std::filesystem::path m_libraryRoot;
};

} // namespace LibriFlow::ManagedStorage
