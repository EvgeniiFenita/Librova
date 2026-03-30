using LibriFlow.UI.Mvvm;

namespace LibriFlow.UI.ViewModels;

internal sealed class ShellWindowViewModel : ObservableObject
{
    private ShellWindowViewModel(string title, ShellViewModel? shell, string statusText, string? startupError)
    {
        Title = title;
        Shell = shell;
        StatusText = statusText;
        StartupError = startupError;
    }

    public string Title { get; }
    public ShellViewModel? Shell { get; }
    public string StatusText { get; }
    public string? StartupError { get; }
    public bool HasShell => Shell is not null;
    public bool HasStartupError => !string.IsNullOrWhiteSpace(StartupError);

    public static ShellWindowViewModel CreateRunning(ShellViewModel shell) =>
        new("LibriFlow", shell, "Connected to native core host.", null);

    public static ShellWindowViewModel CreateStartupError(string message) =>
        new(
            "LibriFlow Startup Error",
            null,
            "Startup failed.",
            string.IsNullOrWhiteSpace(message) ? "Failed to start LibriFlow." : message);
}
