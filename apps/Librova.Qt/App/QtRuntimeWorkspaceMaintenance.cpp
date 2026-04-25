#include "App/QtRuntimeWorkspaceMaintenance.hpp"

#include <filesystem>

#include "Foundation/FileSystemUtils.hpp"

namespace LibrovaQt {

void QtRuntimeWorkspaceMaintenance::PrepareForSession(
    const std::filesystem::path& runtimeWorkspaceRoot)
{
    if (runtimeWorkspaceRoot.empty())
    {
        return;
    }

    CleanupDirectory(runtimeWorkspaceRoot / "ImportWorkspaces" / "GeneratedUiImportWorkspace");
    RemoveIfEmpty(runtimeWorkspaceRoot / "ImportWorkspaces");
    CleanupDirectory(runtimeWorkspaceRoot / "ConverterWorkspace");
    CleanupDirectory(runtimeWorkspaceRoot / "ManagedStorageStaging");
    RemoveIfEmpty(runtimeWorkspaceRoot);
}

void QtRuntimeWorkspaceMaintenance::CleanupDirectory(const std::filesystem::path& path)
{
    Librova::Foundation::RemovePathNoThrow(path);
}

void QtRuntimeWorkspaceMaintenance::RemoveIfEmpty(const std::filesystem::path& path)
{
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec) && !ec)
    {
        // Remove only if empty; ignore errors if non-empty or missing.
        std::filesystem::remove(path, ec);
    }
}

} // namespace LibrovaQt
