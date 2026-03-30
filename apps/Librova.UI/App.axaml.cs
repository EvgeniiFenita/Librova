using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Threading;
using Avalonia.Markup.Xaml;
using Librova.UI.Desktop;
using Librova.UI.Logging;
using Librova.UI.Shell;
using Librova.UI.Views;
using System;
using System.Threading;

namespace Librova.UI;

internal sealed partial class App : Application
{
    private ShellApplication? _shellApplication;

    public override void Initialize()
    {
        AvaloniaXamlLoader.Load(this);
    }

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            desktop.ShutdownRequested += OnShutdownRequested;
            var mainWindow = new MainWindow();
            ShellWindowConfigurator.ConfigureStartingUp(mainWindow);
            desktop.MainWindow = mainWindow;

            StartShellAsync(desktop, mainWindow);
        }

        base.OnFrameworkInitializationCompleted();
    }

    private async void StartShellAsync(IClassicDesktopStyleApplicationLifetime desktop, MainWindow mainWindow)
    {
        try
        {
            UiLogging.Information("Starting Avalonia desktop shell.");
            var session = await ShellBootstrap.StartDevelopmentSessionAsync(CancellationToken.None);
            var launchOptions = ShellLaunchOptions.FromArgs(desktop.Args);
            _shellApplication = ShellApplication.Create(
                session,
                new AvaloniaPathSelectionService(mainWindow),
                launchOptions);
            await _shellApplication.InitializeAsync();
            ShellWindowConfigurator.Configure(mainWindow, _shellApplication);
            UiLogging.Information("Avalonia desktop shell is ready.");
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to initialize Avalonia desktop shell.");
            ShellWindowConfigurator.ConfigureStartupError(mainWindow, error.Message);
        }
    }

    private async void OnShutdownRequested(object? sender, ShutdownRequestedEventArgs e)
    {
        if (_shellApplication is null)
        {
            return;
        }

        UiLogging.Information("Shutting down Avalonia desktop shell.");
        e.Cancel = true;
        var application = _shellApplication;
        _shellApplication = null;

        try
        {
            await application.DisposeAsync();
        }
        finally
        {
            if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            {
                desktop.ShutdownRequested -= OnShutdownRequested;
                Dispatcher.UIThread.Post(() => desktop.Shutdown());
            }
        }
    }
}
