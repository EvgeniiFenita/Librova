#pragma once

#include <filesystem>

#include "Domain/ServiceContracts.hpp"

namespace Librova::ManagedTrash {

class CManagedTrashService final : public Librova::Domain::ITrashService
{
public:
    explicit CManagedTrashService(std::filesystem::path libraryRoot);

    [[nodiscard]] std::filesystem::path MoveToTrash(const std::filesystem::path& path) override;
    void RestoreFromTrash(
        const std::filesystem::path& trashedPath,
        const std::filesystem::path& destinationPath) override;

private:
    [[nodiscard]] std::filesystem::path ResolveManagedPath(const std::filesystem::path& path) const;
    [[nodiscard]] std::filesystem::path BuildTrashDestination(const std::filesystem::path& sourcePath) const;

    std::filesystem::path m_libraryRoot;
};

} // namespace Librova::ManagedTrash
