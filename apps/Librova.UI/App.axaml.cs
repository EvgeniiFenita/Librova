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
using System.Threading.Tasks;

namespace Librova.UI;

internal sealed partial class App : Application
{
    private ShellApplication? _shellApplication;
    private CancellationTokenSource? _startupCancellation;
    private Task? _startupTask;
    private bool _isShuttingDown;

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

            _startupCancellation = new CancellationTokenSource();
            _startupTask = InitializeShellWindowAsync(
                desktop,
                mainWindow,
                pathSelectionService,
                preferencesStore,
                _startupCancellation.Token);
        }

        base.OnFrameworkInitializationCompleted();
    }

    private async Task InitializeShellWindowAsync(
        IClassicDesktopStyleApplicationLifetime desktop,
        MainWindow mainWindow,
        IPathSelectionService pathSelectionService,
        IUiPreferencesStore preferencesStore,
        CancellationToken cancellationToken)
    {
        var launchOptions = ShellLaunchOptions.FromArgs(desktop.Args);
        cancellationToken.ThrowIfCancellationRequested();

        if (FirstRunSetupPolicy.RequiresSetup(preferencesStore))
        {
            UiLogging.Information("Showing first-run setup before host startup.");
            var setup = CreateRecoverySetupViewModel(
                mainWindow,
                pathSelectionService,
                preferencesStore,
                launchOptions,
                CoreHostDevelopmentDefaults.GetFallbackLibraryRoot(),
                cancellationToken);
            ShellWindowConfigurator.ConfigureFirstRunSetup(mainWindow, setup);
            return;
        }

        await StartShellWithLaunchOptionsAsync(
            mainWindow,
            pathSelectionService,
            preferencesStore,
            launchOptions,
            CoreHostDevelopmentDefaults.Create(preferencesStore: preferencesStore),
            cancellationToken);
    }

    private Task StartShellWithLibraryRootAsync(
        MainWindow mainWindow,
        IPathSelectionService pathSelectionService,
        IUiPreferencesStore preferencesStore,
        ShellLaunchOptions launchOptions,
        string libraryRoot,
        CancellationToken cancellationToken)
    {
        ShellWindowConfigurator.ConfigureStartingUp(mainWindow);
        var effectivePreferences = UiPreferencesSnapshotBuilder.WithPreferredLibraryRoot(
            preferencesStore.TryLoad(),
            libraryRoot);
        return StartShellWithLaunchOptionsAsync(
            mainWindow,
            pathSelectionService,
            preferencesStore,
            launchOptions,
            CoreHostDevelopmentDefaults.CreateForLibraryRoot(
                libraryRoot,
                effectivePreferences),
            cancellationToken,
            effectivePreferences);
    }

    private async Task StartShellWithLaunchOptionsAsync(
        MainWindow mainWindow,
        IPathSelectionService pathSelectionService,
        IUiPreferencesStore preferencesStore,
        ShellLaunchOptions launchOptions,
        CoreHostLaunchOptions hostOptions,
        CancellationToken cancellationToken,
        UiPreferencesSnapshot? savedPreferencesOverride = null)
    {
        try
        {
            UiLogging.Information("Starting Avalonia desktop shell.");
            var session = await ShellBootstrap.StartSessionAsync(hostOptions, cancellationToken);
            cancellationToken.ThrowIfCancellationRequested();
            _shellApplication = ShellApplication.Create(
                session,
                pathSelectionService,
                launchOptions,
                preferencesStore: preferencesStore,
                savedPreferencesOverride: savedPreferencesOverride);
            await _shellApplication.InitializeAsync();
            ShellWindowConfigurator.Configure(mainWindow, _shellApplication);
            UiLogging.Information("Avalonia desktop shell is ready.");
        }
        catch (OperationCanceledException) when (_isShuttingDown || cancellationToken.IsCancellationRequested)
        {
            UiLogging.Information("Avalonia desktop shell startup was cancelled.");
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to initialize Avalonia desktop shell.");
            var recoverySetup = CreateRecoverySetupViewModel(
                mainWindow,
                pathSelectionService,
                preferencesStore,
                launchOptions,
                hostOptions.LibraryRoot,
                cancellationToken);
            ShellWindowConfigurator.ConfigureStartupError(mainWindow, error.Message, recoverySetup);
        }
    }

    private FirstRunSetupViewModel CreateRecoverySetupViewModel(
        MainWindow mainWindow,
        IPathSelectionService pathSelectionService,
        IUiPreferencesStore preferencesStore,
        ShellLaunchOptions launchOptions,
        string suggestedLibraryRoot,
        CancellationToken cancellationToken) =>
        new(
            suggestedLibraryRoot,
            pathSelectionService,
            preferencesStore,
            libraryRoot => StartShellWithLibraryRootAsync(
                mainWindow,
                pathSelectionService,
                preferencesStore,
                launchOptions,
                libraryRoot,
                cancellationToken));

    private async void OnShutdownRequested(object? sender, ShutdownRequestedEventArgs e)
    {
        e.Cancel = true;
        _isShuttingDown = true;
        UiLogging.Information("Shutting down Avalonia desktop shell.");
        _startupCancellation?.Cancel();
        var application = _shellApplication;
        var startupTask = _startupTask;
        _shellApplication = null;
        _startupTask = null;

        try
        {
            if (startupTask is not null)
            {
                try
                {
                    await startupTask;
                }
                catch (OperationCanceledException)
                {
                }
            }

            if (application is not null)
            {
                await application.DisposeAsync();
            }
        }
        finally
        {
            _startupCancellation?.Dispose();
            _startupCancellation = null;
            if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            {
                desktop.ShutdownRequested -= OnShutdownRequested;
                Dispatcher.UIThread.Post(() => desktop.Shutdown());
            }
        }
    }
}
