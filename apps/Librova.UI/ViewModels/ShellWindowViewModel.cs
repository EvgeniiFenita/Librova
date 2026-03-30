using Librova.UI.Mvvm;
using Librova.UI.Runtime;

namespace Librova.UI.ViewModels;

internal sealed class ShellWindowViewModel : ObservableObject
{
    private ShellWindowViewModel(
        string title,
        ShellViewModel? shell,
        string statusText,
        string? startupError,
        string uiLogFilePath,
        string uiStateFilePath,
        string uiPreferencesFilePath,
        string startupGuidanceText)
    {
        Title = title;
        Shell = shell;
        StatusText = statusText;
        StartupError = startupError;
        UiLogFilePath = uiLogFilePath;
        UiStateFilePath = uiStateFilePath;
        UiPreferencesFilePath = uiPreferencesFilePath;
        StartupGuidanceText = startupGuidanceText;
    }

    public string Title { get; }
    public ShellViewModel? Shell { get; }
    public string StatusText { get; }
    public string? StartupError { get; }
    public string UiLogFilePath { get; }
    public string UiStateFilePath { get; }
    public string UiPreferencesFilePath { get; }
    public string StartupGuidanceText { get; }
    public bool HasShell => Shell is not null;
    public bool HasStartupError => !string.IsNullOrWhiteSpace(StartupError);
    public bool IsStartingUp => !HasShell && !HasStartupError;

    public static ShellWindowViewModel CreateStartingUp() =>
        new(
            "Librova",
            null,
            "Starting native core host...",
            null,
            RuntimeEnvironment.GetDefaultUiLogFilePath(),
            RuntimeEnvironment.GetDefaultUiStateFilePath(),
            RuntimeEnvironment.GetDefaultUiPreferencesFilePath(),
            "The app is preparing the native core host and loading the first library snapshot.");

    public static ShellWindowViewModel CreateRunning(ShellViewModel shell) =>
        new(
            "Librova",
            shell,
            "Connected to native core host.",
            null,
            shell.UiLogFilePath,
            shell.UiStateFilePath,
            shell.UiPreferencesFilePath,
            string.Empty);

    public static ShellWindowViewModel CreateStartupError(string message) =>
        new(
            "Librova Startup Error",
            null,
            "Startup failed.",
            string.IsNullOrWhiteSpace(message) ? "Failed to start Librova." : message,
            RuntimeEnvironment.GetDefaultUiLogFilePath(),
            RuntimeEnvironment.GetDefaultUiStateFilePath(),
            RuntimeEnvironment.GetDefaultUiPreferencesFilePath(),
            "Check the UI log first. If the issue is related to library bootstrap or host startup, verify the configured library root and then retry the app.");
}
