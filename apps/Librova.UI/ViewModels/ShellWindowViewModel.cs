using Librova.UI.Mvvm;
using Librova.UI.Logging;
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
    public bool HasStartupRecoverySetup => HasStartupError && Setup is not null;
    public bool IsShowingFirstRunSetup => HasSetup && !HasStartupError;
    public bool IsStartingUp => !HasShell && !HasStartupError && !HasSetup;

    public static ShellWindowViewModel CreateStartingUp() =>
        new(
            "Librova",
            null,
            null,
            "Starting native core host...",
            null,
            UiLogging.CurrentLogFilePath ?? RuntimeEnvironment.GetDefaultUiLogFilePath(),
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
            "Choose whether to create a new Librova library or open an existing one before the first launch.",
            null,
            UiLogging.CurrentLogFilePath ?? RuntimeEnvironment.GetDefaultUiLogFilePath(),
            RuntimeEnvironment.GetDefaultUiStateFilePath(),
            RuntimeEnvironment.GetDefaultUiPreferencesFilePath(),
            "Librova stores the managed library, database, covers, logs, temporary import files, and trash under the selected root. You can create a new library here, or point Librova at an existing managed library and continue with that catalog.");

    public static ShellWindowViewModel CreateStartupError(string message, FirstRunSetupViewModel? recoverySetup = null) =>
        new(
            "Librova Startup Error",
            null,
            recoverySetup,
            "Startup failed.",
            string.IsNullOrWhiteSpace(message) ? "Failed to start Librova." : message,
            UiLogging.CurrentLogFilePath ?? RuntimeEnvironment.GetDefaultUiLogFilePath(),
            RuntimeEnvironment.GetDefaultUiStateFilePath(),
            RuntimeEnvironment.GetDefaultUiPreferencesFilePath(),
            recoverySetup is null
                ? "Check the UI log first. If the issue is related to library bootstrap or host startup, verify the configured library root and then retry the app."
                : "Check the UI log first. You can immediately choose a different library root below and retry startup without leaving the app.");
}
