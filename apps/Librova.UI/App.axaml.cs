using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Threading;
using Avalonia.Markup.Xaml;
using Librova.UI.Desktop;
using Librova.UI.CoreHost;
using Librova.UI.Logging;
using Librova.UI.Shell;
using Librova.UI.ViewModels;
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
            var pathSelectionService = new AvaloniaPathSelectionService(mainWindow);
            var preferencesStore = UiPreferencesStore.CreateDefault();
            ShellWindowConfigurator.ConfigureStartingUp(mainWindow);
            desktop.MainWindow = mainWindow;

            InitializeShellWindowAsync(desktop, mainWindow, pathSelectionService, preferencesStore);
        }

        base.OnFrameworkInitializationCompleted();
    }

    private async void InitializeShellWindowAsync(
        IClassicDesktopStyleApplicationLifetime desktop,
        MainWindow mainWindow,
        IPathSelectionService pathSelectionService,
        IUiPreferencesStore preferencesStore)
    {
        var launchOptions = ShellLaunchOptions.FromArgs(desktop.Args);

        if (FirstRunSetupPolicy.RequiresSetup(preferencesStore))
        {
            UiLogging.Information("Showing first-run setup before host startup.");
            var setup = CreateRecoverySetupViewModel(
                mainWindow,
                pathSelectionService,
                preferencesStore,
                launchOptions,
                CoreHostDevelopmentDefaults.GetFallbackLibraryRoot());
            ShellWindowConfigurator.ConfigureFirstRunSetup(mainWindow, setup);
            return;
        }

        await StartShellWithLaunchOptionsAsync(
            mainWindow,
            pathSelectionService,
            preferencesStore,
            launchOptions,
            CoreHostDevelopmentDefaults.Create(preferencesStore: preferencesStore));
    }

    private Task StartShellWithLibraryRootAsync(
        MainWindow mainWindow,
        IPathSelectionService pathSelectionService,
        IUiPreferencesStore preferencesStore,
        ShellLaunchOptions launchOptions,
        string libraryRoot)
    {
        ShellWindowConfigurator.ConfigureStartingUp(mainWindow);
        return StartShellWithLaunchOptionsAsync(
            mainWindow,
            pathSelectionService,
            preferencesStore,
            launchOptions,
            CoreHostDevelopmentDefaults.CreateForLibraryRoot(
                libraryRoot,
                preferencesStore.TryLoad()));
    }

    private async Task StartShellWithLaunchOptionsAsync(
        MainWindow mainWindow,
        IPathSelectionService pathSelectionService,
        IUiPreferencesStore preferencesStore,
        ShellLaunchOptions launchOptions,
        CoreHostLaunchOptions hostOptions)
    {
        try
        {
            UiLogging.Information("Starting Avalonia desktop shell.");
            var session = await ShellBootstrap.StartSessionAsync(hostOptions, CancellationToken.None);
            _shellApplication = ShellApplication.Create(
                session,
                pathSelectionService,
                launchOptions,
                preferencesStore: preferencesStore);
            await _shellApplication.InitializeAsync();
            ShellWindowConfigurator.Configure(mainWindow, _shellApplication);
            UiLogging.Information("Avalonia desktop shell is ready.");
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to initialize Avalonia desktop shell.");
            var recoverySetup = CreateRecoverySetupViewModel(
                mainWindow,
                pathSelectionService,
                preferencesStore,
                launchOptions,
                hostOptions.LibraryRoot);
            ShellWindowConfigurator.ConfigureStartupError(mainWindow, error.Message, recoverySetup);
        }
    }

    private FirstRunSetupViewModel CreateRecoverySetupViewModel(
        MainWindow mainWindow,
        IPathSelectionService pathSelectionService,
        IUiPreferencesStore preferencesStore,
        ShellLaunchOptions launchOptions,
        string suggestedLibraryRoot) =>
        new(
            suggestedLibraryRoot,
            pathSelectionService,
            preferencesStore,
            libraryRoot => StartShellWithLibraryRootAsync(
                mainWindow,
                pathSelectionService,
                preferencesStore,
                launchOptions,
                libraryRoot));

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
