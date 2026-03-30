using Librova.UI.Views;
using Librova.UI.ViewModels;

namespace Librova.UI.Shell;

internal static class ShellWindowConfigurator
{
    public static ShellWindowState CreateStartingUpState() =>
        new(ShellWindowViewModel.CreateStartingUp());

    public static ShellWindowState CreateState(ShellApplication application) =>
        new(ShellWindowViewModel.CreateRunning(application.Shell));

    public static ShellWindowState CreateFirstRunSetupState(FirstRunSetupViewModel setup) =>
        new(ShellWindowViewModel.CreateFirstRunSetup(setup));

    public static ShellWindowState CreateStartupErrorState(string message) =>
        new(ShellWindowViewModel.CreateStartupError(message));

    public static void Configure(MainWindow window, ShellApplication application)
    {
        var state = CreateState(application);
        Apply(window, state);
    }

    public static void ConfigureFirstRunSetup(MainWindow window, FirstRunSetupViewModel setup) =>
        Apply(window, CreateFirstRunSetupState(setup));

    public static void ConfigureStartupError(MainWindow window, string message) =>
        Apply(window, CreateStartupErrorState(message));

    public static void ConfigureStartingUp(MainWindow window) =>
        Apply(window, CreateStartingUpState());

    private static void Apply(MainWindow window, ShellWindowState state)
    {
        window.Title = state.ViewModel.Title;
        window.DataContext = state.ViewModel;
    }
}

internal sealed record ShellWindowState(ShellWindowViewModel ViewModel);
