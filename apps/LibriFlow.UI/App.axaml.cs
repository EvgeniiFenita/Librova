using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using LibriFlow.UI.Logging;
using LibriFlow.UI.Shell;
using LibriFlow.UI.Views;
using System;
using System.Threading;

namespace LibriFlow.UI;

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

            try
            {
                UiLogging.Information("Starting Avalonia desktop shell.");
                _shellApplication = ShellApplication.StartDevelopmentAsync(CancellationToken.None)
                    .GetAwaiter()
                    .GetResult();

                var mainWindow = new MainWindow();
                ShellWindowConfigurator.Configure(mainWindow, _shellApplication);
                desktop.MainWindow = mainWindow;
                UiLogging.Information("Avalonia desktop shell is ready.");
            }
            catch (Exception error)
            {
                UiLogging.Error(error, "Failed to initialize Avalonia desktop shell.");
                var mainWindow = new MainWindow();
                ShellWindowConfigurator.ConfigureStartupError(mainWindow, error.Message);
                desktop.MainWindow = mainWindow;
            }
        }

        base.OnFrameworkInitializationCompleted();
    }

    private void OnShutdownRequested(object? sender, ShutdownRequestedEventArgs e)
    {
        if (_shellApplication is null)
        {
            return;
        }

        UiLogging.Information("Shutting down Avalonia desktop shell.");
        _shellApplication.DisposeAsync().AsTask().GetAwaiter().GetResult();
        _shellApplication = null;
    }
}
