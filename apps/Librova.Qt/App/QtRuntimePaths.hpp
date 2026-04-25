#pragma once

#include <filesystem>

#include <QString>

namespace LibrovaQt {

// Static helpers for resolving runtime file/directory paths.
//
// Portable mode: a 'librova-portable' marker file exists next to the executable.
// In portable mode data lives under <exeDir>/PortableData/.
// In dev/installed mode data lives under the user's AppData/Local/Librova/.
class QtRuntimePaths
{
public:
    static constexpr const char* PortableMarkerFileName = "librova-portable";
    static constexpr const char* PortableDataDirName = "PortableData";
    static constexpr const char* AppDataDirName = "Librova";

    [[nodiscard]] static bool IsPortableMode(const QString& exeDir);

    [[nodiscard]] static QString DefaultPreferencesFilePath(const QString& exeDir);
    [[nodiscard]] static QString DefaultShellStateFilePath(const QString& exeDir);
    [[nodiscard]] static QString DefaultLogFilePath(const QString& exeDir);
    [[nodiscard]] static std::filesystem::path UiLogFilePathForLibrary(
        const std::filesystem::path& libraryRoot);
    [[nodiscard]] static std::filesystem::path UiRuntimeLogFilePathForLibrary(
        const std::filesystem::path& libraryRoot,
        const QString& exeDir);

    // Runtime workspace root for a given library (for converter/import staging).
    // Keep the runtime workspace stable for a given library root.
    [[nodiscard]] static std::filesystem::path RuntimeWorkspaceRoot(
        const std::filesystem::path& libraryRoot,
        const QString& exeDir);

    [[nodiscard]] static std::filesystem::path ConverterWorkingDirectory(
        const std::filesystem::path& libraryRoot,
        const QString& exeDir);

    [[nodiscard]] static std::filesystem::path ManagedStorageStagingRoot(
        const std::filesystem::path& libraryRoot,
        const QString& exeDir);

    [[nodiscard]] static QString ResolvePreferredLibraryRoot(
        const QString& preferredLibraryRoot,
        const QString& portablePreferredLibraryRoot,
        const QString& exeDir);
    [[nodiscard]] static QString BuildPortableLibraryRootPreference(
        const QString& libraryRoot,
        const QString& exeDir);
    static void SyncRuntimeLogFile(
        const std::filesystem::path& runtimeLogPath,
        const std::filesystem::path& retainedLogPath);

private:
    [[nodiscard]] static QString DataRootDir(const QString& exeDir);
    [[nodiscard]] static QString LocalAppDataDir();
    [[nodiscard]] static QString EnsureTrailingSeparator(const QString& path);
};

} // namespace LibrovaQt
