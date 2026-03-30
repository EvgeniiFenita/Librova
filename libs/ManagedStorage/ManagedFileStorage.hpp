#pragma once

#include <filesystem>

#include "Domain/ServiceContracts.hpp"

namespace Librova::ManagedStorage {

class CManagedFileStorage final : public Librova::Domain::IManagedStorage
{
public:
    explicit CManagedFileStorage(std::filesystem::path libraryRoot);

    [[nodiscard]] Librova::Domain::SPreparedStorage PrepareImport(const Librova::Domain::SStoragePlan& plan) override;
    void CommitImport(const Librova::Domain::SPreparedStorage& preparedStorage) override;
    void RollbackImport(const Librova::Domain::SPreparedStorage& preparedStorage) noexcept override;

private:
    std::filesystem::path m_libraryRoot;
};

} // namespace Librova::ManagedStorage
