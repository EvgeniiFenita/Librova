using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Threading;
using Avalonia.Markup.Xaml;
using Librova.UI.Desktop;
using Librova.UI.CoreHost;
using Librova.UI.Logging;
using Librova.UI.Runtime;
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
                cancellationToken,
                explicitOpenMode: UiLibraryOpenMode.CreateNew);
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
        CancellationToken cancellationToken,
        UiLibraryOpenMode libraryOpenMode)
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
                effectivePreferences,
                libraryOpenMode: libraryOpenMode),
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
        var validationMessage = LibraryRootValidation.BuildValidationMessage(hostOptions.LibraryRoot);
        if (!string.IsNullOrWhiteSpace(validationMessage))
        {
            UiLogging.Warning(
                "Skipping host startup because the configured library root is invalid. LibraryRoot={LibraryRoot} Validation={Validation}",
                hostOptions.LibraryRoot,
                validationMessage);
            var recoverySetup = CreateRecoverySetupViewModel(
                mainWindow,
                pathSelectionService,
                preferencesStore,
                launchOptions,
                hostOptions.LibraryRoot,
                cancellationToken);
            ShellWindowConfigurator.ConfigureStartupError(
                mainWindow,
                $"Configured library root '{hostOptions.LibraryRoot}' is invalid: {validationMessage}",
                recoverySetup);
            return;
        }

        try
        {
            UiLogging.Information("Starting Avalonia desktop shell.");
            var session = await ShellBootstrap.StartSessionAsync(hostOptions, cancellationToken);
            cancellationToken.ThrowIfCancellationRequested();
            UiLogging.Reinitialize(RuntimeEnvironment.GetUiLogFilePathForLibrary(hostOptions.LibraryRoot));
            _shellApplication = ShellApplication.Create(
                session,
                pathSelectionService,
                launchOptions,
                preferencesStore: preferencesStore,
                savedPreferencesOverride: savedPreferencesOverride,
                switchLibraryAsync: (libraryRoot, libraryOpenMode) => SwitchLibraryRootAsync(
                    mainWindow,
                    pathSelectionService,
                    preferencesStore,
                    launchOptions,
                    libraryRoot,
                    cancellationToken,
                    libraryOpenMode),
                reloadShellAsync: () => ReloadCurrentShellSessionAsync(
                    mainWindow,
                    pathSelectionService,
                    preferencesStore,
                    launchOptions,
                    cancellationToken));
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
                cancellationToken,
                requireDifferentLibraryRoot: true);
            ShellWindowConfigurator.ConfigureStartupError(mainWindow, error.Message, recoverySetup);
        }
    }

    private FirstRunSetupViewModel CreateRecoverySetupViewModel(
        MainWindow mainWindow,
        IPathSelectionService pathSelectionService,
        IUiPreferencesStore preferencesStore,
        ShellLaunchOptions launchOptions,
        string suggestedLibraryRoot,
        CancellationToken cancellationToken,
        UiLibraryOpenMode? explicitOpenMode = null,
        bool requireDifferentLibraryRoot = false) =>
        new(
            suggestedLibraryRoot,
            pathSelectionService,
            preferencesStore,
            libraryRoot =>
            {
                var resolvedMode = explicitOpenMode ?? LibraryRootInspection.ResolveModeForRecovery(libraryRoot);
                return StartShellWithLibraryRootAsync(
                    mainWindow,
                    pathSelectionService,
                    preferencesStore,
                    launchOptions,
                    libraryRoot,
                    cancellationToken,
                    resolvedMode);
            },
            requireDifferentLibraryRoot,
            explicitOpenMode is null
                ? null
                : libraryRoot => LibraryRootInspection.BuildModeValidationMessage(libraryRoot, explicitOpenMode.Value));

    private async Task SwitchLibraryRootAsync(
        MainWindow mainWindow,
        IPathSelectionService pathSelectionService,
        IUiPreferencesStore preferencesStore,
        ShellLaunchOptions launchOptions,
        string libraryRoot,
        CancellationToken cancellationToken,
        UiLibraryOpenMode libraryOpenMode)
    {
        UiLogging.Information("Switching active library. LibraryRoot={LibraryRoot}", libraryRoot);
        ShellWindowConfigurator.ConfigureStartingUp(mainWindow);

        var previousApplication = _shellApplication;
        _shellApplication = null;
        if (previousApplication is not null)
        {
            await previousApplication.DisposeAsync();
        }

        await StartShellWithLibraryRootAsync(
            mainWindow,
            pathSelectionService,
            preferencesStore,
            launchOptions,
            libraryRoot,
            cancellationToken,
            libraryOpenMode);

        if (_shellApplication is not null)
        {
            preferencesStore.Save(UiPreferencesSnapshotBuilder.WithPreferredLibraryRoot(
                preferencesStore.TryLoad(),
                libraryRoot));
        }
    }

    private async Task ReloadCurrentShellSessionAsync(
        MainWindow mainWindow,
        IPathSelectionService pathSelectionService,
        IUiPreferencesStore preferencesStore,
        ShellLaunchOptions launchOptions,
        CancellationToken cancellationToken)
    {
        var libraryRoot = _shellApplication?.Shell.LibraryRoot;
        var currentSection = _shellApplication?.Shell.CurrentSection ?? ShellSection.Library;
        if (string.IsNullOrWhiteSpace(libraryRoot))
        {
            return;
        }

        UiLogging.Information("Reloading shell session to apply updated preferences. LibraryRoot={LibraryRoot}", libraryRoot);

        // Do NOT call ConfigureStartingUp here.
        // ConfigureStartingUp sets Shell = null on the DataContext (HasShell = false), which hides
        // the shell grid and causes Avalonia compiled binding chains (Shell.LibraryRoot,
        // all SettingsView bindings, etc.) to go blank. When the new Running DataContext is set
        // afterwards, the bindings on previously-hidden elements do not recover.
        // For a preferences reload the shell grid must stay visible at all times so that
        // we transition directly from the old Running ViewModel to the new one.
        var previousApplication = _shellApplication;
        _shellApplication = null;
        if (previousApplication is not null)
        {
            await previousApplication.DisposeAsync();
        }

        var effectivePreferences = UiPreferencesSnapshotBuilder.WithPreferredLibraryRoot(
            preferencesStore.TryLoad(),
            libraryRoot);
        await StartShellWithLaunchOptionsAsync(
            mainWindow,
            pathSelectionService,
            preferencesStore,
            launchOptions,
            CoreHostDevelopmentDefaults.CreateForLibraryRoot(
                libraryRoot,
                effectivePreferences,
                libraryOpenMode: UiLibraryOpenMode.OpenExisting),
            cancellationToken,
            effectivePreferences);

        if (_shellApplication is not null && currentSection is not ShellSection.Library)
        {
            await _shellApplication.Shell.ActivateSectionAsync(currentSection);
        }
    }

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
