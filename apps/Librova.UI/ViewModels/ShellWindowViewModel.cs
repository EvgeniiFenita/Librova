using Librova.UI.Mvvm;
using Librova.UI.Runtime;

namespace Librova.UI.ViewModels;

internal sealed class ShellWindowViewModel : ObservableObject
{
    private ShellWindowViewModel(
        string title,
        ShellViewModel? shell,
        FirstRunSetupViewModel? setup,
        string statusText,
        string? startupError,
        string uiLogFilePath,
        string uiStateFilePath,
        string uiPreferencesFilePath,
        string startupGuidanceText)
    {
        Title = title;
        Shell = shell;
        Setup = setup;
        StatusText = statusText;
        StartupError = startupError;
        UiLogFilePath = uiLogFilePath;
        UiStateFilePath = uiStateFilePath;
        UiPreferencesFilePath = uiPreferencesFilePath;
        StartupGuidanceText = startupGuidanceText;
    }

    public string Title { get; }
    public ShellViewModel? Shell { get; }
    public FirstRunSetupViewModel? Setup { get; }
    public string StatusText { get; }
    public string? StartupError { get; }
    public string UiLogFilePath { get; }
    public string UiStateFilePath { get; }
    public string UiPreferencesFilePath { get; }
    public string StartupGuidanceText { get; }
    public bool HasShell => Shell is not null;
    public bool HasSetup => Setup is not null;
    public bool HasStartupError => !string.IsNullOrWhiteSpace(StartupError);
    public bool IsStartingUp => !HasShell && !HasStartupError && !HasSetup;

    public static ShellWindowViewModel CreateStartingUp() =>
        new(
            "Librova",
            null,
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
            null,
            "Connected to native core host.",
            null,
            shell.UiLogFilePath,
            shell.UiStateFilePath,
            shell.UiPreferencesFilePath,
            string.Empty);

    public static ShellWindowViewModel CreateFirstRunSetup(FirstRunSetupViewModel setup) =>
        new(
            "Librova Setup",
            null,
            setup,
            "Choose the managed library location before the first launch.",
            null,
            RuntimeEnvironment.GetDefaultUiLogFilePath(),
            RuntimeEnvironment.GetDefaultUiStateFilePath(),
            RuntimeEnvironment.GetDefaultUiPreferencesFilePath(),
            "Librova stores the managed library, database, covers, logs, temporary import files, and trash under the selected root. You can change this again later in the running shell settings.");

    public static ShellWindowViewModel CreateStartupError(string message) =>
        new(
            "Librova Startup Error",
            null,
            null,
            "Startup failed.",
            string.IsNullOrWhiteSpace(message) ? "Failed to start Librova." : message,
            RuntimeEnvironment.GetDefaultUiLogFilePath(),
            RuntimeEnvironment.GetDefaultUiStateFilePath(),
            RuntimeEnvironment.GetDefaultUiPreferencesFilePath(),
            "Check the UI log first. If the issue is related to library bootstrap or host startup, verify the configured library root and then retry the app.");
}
