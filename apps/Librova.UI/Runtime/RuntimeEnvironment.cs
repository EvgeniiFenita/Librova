using System;
using System.IO;

namespace Librova.UI.Runtime;

internal static class RuntimeEnvironment
{
    public const string UiLogFileEnvVar = "LIBROVA_UI_LOG_FILE";
    public const string UiStateFileEnvVar = "LIBROVA_UI_STATE_FILE";
    public const string UiPreferencesFileEnvVar = "LIBROVA_UI_PREFERENCES_FILE";
    public const string LibraryRootEnvVar = "LIBROVA_LIBRARY_ROOT";
    public const string CoreHostExecutableEnvVar = "LIBROVA_CORE_HOST_EXECUTABLE";

    public static string GetDefaultUiLogFilePath() =>
        GetPathFromEnvironment(UiLogFileEnvVar)
        ?? Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "Librova",
            "Logs",
            "ui.log");

    public static string GetDefaultUiStateFilePath() =>
        GetPathFromEnvironment(UiStateFileEnvVar)
        ?? Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "Librova",
            "ui-shell-state.json");

    public static string GetDefaultUiPreferencesFilePath() =>
        GetPathFromEnvironment(UiPreferencesFileEnvVar)
        ?? Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "Librova",
            "ui-preferences.json");

    public static string? GetLibraryRootOverride() => GetPathFromEnvironment(LibraryRootEnvVar);

    public static string? GetCoreHostExecutableOverride() => GetPathFromEnvironment(CoreHostExecutableEnvVar);

    private static string? GetPathFromEnvironment(string variableName)
    {
        var value = Environment.GetEnvironmentVariable(variableName);
        return string.IsNullOrWhiteSpace(value) ? null : value;
    }
}
