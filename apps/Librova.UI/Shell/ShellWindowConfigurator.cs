using Librova.UI.Views;
using Librova.UI.ViewModels;

namespace Librova.UI.Shell;

internal static class ShellWindowConfigurator
{
    public static ShellWindowState CreateState(ShellApplication application) =>
        new(ShellWindowViewModel.CreateRunning(application.Shell));

    public static ShellWindowState CreateStartupErrorState(string message) =>
        new(ShellWindowViewModel.CreateStartupError(message));

    public static void Configure(MainWindow window, ShellApplication application)
    {
        var state = CreateState(application);
        Apply(window, state);
    }

    public static void ConfigureStartupError(MainWindow window, string message) =>
        Apply(window, CreateStartupErrorState(message));

    private static void Apply(MainWindow window, ShellWindowState state)
    {
        window.Title = state.ViewModel.Title;
        window.DataContext = state.ViewModel;
    }
}

internal sealed record ShellWindowState(ShellWindowViewModel ViewModel);
