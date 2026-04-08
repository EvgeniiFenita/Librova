using System;
using System.IO;

namespace Librova.UI.Runtime;

internal static class RuntimeEnvironment
{
    private const string PackagedHostExecutableName = "LibrovaCoreHostApp.exe";
    private const string PortableDataDirectoryName = "PortableData";

    public const string UiLogFileEnvVar = "LIBROVA_UI_LOG_FILE";
    public const string UiStateFileEnvVar = "LIBROVA_UI_STATE_FILE";
    public const string UiPreferencesFileEnvVar = "LIBROVA_UI_PREFERENCES_FILE";
    public const string LibraryRootEnvVar = "LIBROVA_LIBRARY_ROOT";
    public const string CoreHostExecutableEnvVar = "LIBROVA_CORE_HOST_EXECUTABLE";

    public static string GetUiLogFilePathForLibrary(string libraryRoot) =>
        Path.Combine(libraryRoot, "Logs", "ui.log");

    public static string GetDefaultUiLogFilePath() =>
        GetPathFromEnvironment(UiLogFileEnvVar)
        ?? GetDefaultPortableAwarePath(
            Path.Combine("Logs", "ui.log"),
            GetLocalAppDataPath(Path.Combine("Logs", "ui.log")));

    public static string GetDefaultUiStateFilePath() =>
        GetPathFromEnvironment(UiStateFileEnvVar)
        ?? GetDefaultPortableAwarePath(
            "ui-shell-state.json",
            GetLocalAppDataPath("ui-shell-state.json"));

    public static string GetDefaultUiPreferencesFilePath() =>
        GetPathFromEnvironment(UiPreferencesFileEnvVar)
        ?? GetDefaultPortableAwarePath(
            "ui-preferences.json",
            GetLocalAppDataPath("ui-preferences.json"));

    public static string? GetLibraryRootOverride() => GetPathFromEnvironment(LibraryRootEnvVar);

    public static string? GetCoreHostExecutableOverride() => GetPathFromEnvironment(CoreHostExecutableEnvVar);

    internal static string? BuildPortableLibraryRootPreference(
        string libraryRoot,
        string? baseDirectory = null,
        string? hostExecutableOverride = null)
    {
        if (string.IsNullOrWhiteSpace(libraryRoot) || !Path.IsPathFullyQualified(libraryRoot))
        {
            return null;
        }

        if (!IsPortableMode(baseDirectory, hostExecutableOverride))
        {
            return null;
        }

        var effectiveBaseDirectory = Path.GetFullPath(baseDirectory ?? AppContext.BaseDirectory);
        var fullLibraryRoot = Path.GetFullPath(libraryRoot);
        if (!IsContainedWithinBaseDirectory(effectiveBaseDirectory, fullLibraryRoot))
        {
            return null;
        }

        var relativePath = Path.GetRelativePath(effectiveBaseDirectory, fullLibraryRoot);
        return Path.IsPathFullyQualified(relativePath)
            ? null
            : relativePath;
    }

    internal static string? ResolvePreferredLibraryRoot(
        string? preferredLibraryRoot,
        string? portablePreferredLibraryRoot,
        string? baseDirectory = null,
        string? hostExecutableOverride = null)
    {
        if (IsPortableMode(baseDirectory, hostExecutableOverride)
            && !string.IsNullOrWhiteSpace(portablePreferredLibraryRoot))
        {
            if (Path.IsPathFullyQualified(portablePreferredLibraryRoot))
            {
                return preferredLibraryRoot;
            }

            var effectiveBaseDirectory = Path.GetFullPath(baseDirectory ?? AppContext.BaseDirectory);
            var resolvedPortableRoot = Path.GetFullPath(Path.Combine(effectiveBaseDirectory, portablePreferredLibraryRoot));
            return IsContainedWithinBaseDirectory(effectiveBaseDirectory, resolvedPortableRoot)
                ? resolvedPortableRoot
                : preferredLibraryRoot;
        }

        return preferredLibraryRoot;
    }

    internal static bool IsPortableMode(string? baseDirectory = null, string? hostExecutableOverride = null)
    {
        if (!string.IsNullOrWhiteSpace(hostExecutableOverride))
        {
            return false;
        }

        var effectiveBaseDirectory = Path.GetFullPath(baseDirectory ?? AppContext.BaseDirectory);
        var packagedHostPath = Path.Combine(effectiveBaseDirectory, PackagedHostExecutableName);
        return File.Exists(packagedHostPath);
    }

    internal static string GetDefaultUiLogFilePath(string baseDirectory, string? hostExecutableOverride) =>
        GetDefaultPortableAwarePath(
            Path.Combine("Logs", "ui.log"),
            GetLocalAppDataPath(Path.Combine("Logs", "ui.log")),
            baseDirectory,
            hostExecutableOverride);

    internal static string GetDefaultUiStateFilePath(string baseDirectory, string? hostExecutableOverride) =>
        GetDefaultPortableAwarePath(
            "ui-shell-state.json",
            GetLocalAppDataPath("ui-shell-state.json"),
            baseDirectory,
            hostExecutableOverride);

    internal static string GetDefaultUiPreferencesFilePath(string baseDirectory, string? hostExecutableOverride) =>
        GetDefaultPortableAwarePath(
            "ui-preferences.json",
            GetLocalAppDataPath("ui-preferences.json"),
            baseDirectory,
            hostExecutableOverride);

    private static string GetDefaultPortableAwarePath(string relativePortablePath, string localAppDataPath)
        => GetDefaultPortableAwarePath(
            relativePortablePath,
            localAppDataPath,
            AppContext.BaseDirectory,
            GetCoreHostExecutableOverride());

    private static string GetDefaultPortableAwarePath(
        string relativePortablePath,
        string localAppDataPath,
        string baseDirectory,
        string? hostExecutableOverride)
    {
        if (!IsPortableMode(baseDirectory, hostExecutableOverride))
        {
            return localAppDataPath;
        }

        return Path.Combine(
            Path.GetFullPath(baseDirectory),
            PortableDataDirectoryName,
            relativePortablePath);
    }

    private static string GetLocalAppDataPath(string relativePath) =>
        Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "Librova",
            relativePath);

    private static string? GetPathFromEnvironment(string variableName)
    {
        var value = Environment.GetEnvironmentVariable(variableName);
        return string.IsNullOrWhiteSpace(value) ? null : value;
    }

    private static bool IsContainedWithinBaseDirectory(string baseDirectory, string candidatePath)
    {
        var normalizedBaseDirectory = EnsureTrailingDirectorySeparator(Path.GetFullPath(baseDirectory));
        var normalizedCandidatePath = EnsureTrailingDirectorySeparator(Path.GetFullPath(candidatePath));
        return normalizedCandidatePath.StartsWith(normalizedBaseDirectory, StringComparison.OrdinalIgnoreCase);
    }

    private static string EnsureTrailingDirectorySeparator(string path) =>
        path.EndsWith(Path.DirectorySeparatorChar)
            || path.EndsWith(Path.AltDirectorySeparatorChar)
                ? path
                : path + Path.DirectorySeparatorChar;
}
