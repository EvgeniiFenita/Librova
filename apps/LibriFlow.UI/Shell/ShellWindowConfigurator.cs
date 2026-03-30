using LibriFlow.UI.Views;
using LibriFlow.UI.ViewModels;

namespace LibriFlow.UI.Shell;

internal static class ShellWindowConfigurator
{
    public static ShellWindowState CreateState(ShellApplication application) =>
        new("LibriFlow", application.Shell);

    public static void Configure(MainWindow window, ShellApplication application)
    {
        var state = CreateState(application);
        window.Title = state.Title;
        window.DataContext = state.DataContext;
    }
}

internal sealed record ShellWindowState(string Title, ShellViewModel DataContext);
