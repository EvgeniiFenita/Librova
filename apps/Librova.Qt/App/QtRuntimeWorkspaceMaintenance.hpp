#pragma once

#include <filesystem>

namespace LibrovaQt {

// Cleans up transient runtime workspace directories before opening a new library session.
// Prepares the per-session runtime workspace before the library backend opens.
class QtRuntimeWorkspaceMaintenance
{
public:
    static void PrepareForSession(const std::filesystem::path& runtimeWorkspaceRoot);

private:
    static void CleanupDirectory(const std::filesystem::path& path);
    static void RemoveIfEmpty(const std::filesystem::path& path);
};

} // namespace LibrovaQt
