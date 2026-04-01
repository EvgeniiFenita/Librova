using Librova.UI.Desktop;
using Librova.UI.Logging;
using Librova.UI.ViewModels;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.Shell;

internal sealed class ShellApplication : IAsyncDisposable
{
    private readonly ShellSession _session;
    private readonly IShellStateStore _stateStore;

    public ShellApplication(ShellSession session, ShellViewModel shellViewModel, IShellStateStore stateStore)
    {
        _session = session;
        _stateStore = stateStore;
        Shell = shellViewModel;
    }

    public ShellViewModel Shell { get; }

    public static async Task<ShellApplication> StartDevelopmentAsync(CancellationToken cancellationToken)
    {
        var session = await ShellBootstrap.StartDevelopmentSessionAsync(cancellationToken).ConfigureAwait(false);
        var application = Create(session);
        await application.InitializeAsync().ConfigureAwait(false);
        return application;
    }

    public static ShellApplication Create(
        ShellSession session,
        IPathSelectionService? pathSelectionService = null,
        ShellLaunchOptions? launchOptions = null,
        IShellStateStore? stateStore = null,
        IUiPreferencesStore? preferencesStore = null,
        UiPreferencesSnapshot? savedPreferencesOverride = null,
        Func<string, Task>? switchLibraryAsync = null,
        Func<Task>? reloadShellAsync = null)
    {
        var effectiveStateStore = stateStore ?? ShellStateStore.CreateDefault();
        var effectivePreferencesStore = preferencesStore ?? UiPreferencesStore.CreateDefault();
        var savedState = effectiveStateStore.TryLoad();
        var savedPreferences = savedPreferencesOverride ?? effectivePreferencesStore.TryLoad();
        return new(
            session,
            new ShellViewModel(
                session,
                pathSelectionService,
                launchOptions,
                savedState,
                effectivePreferencesStore,
                savedPreferences,
                switchLibraryAsync,
                reloadShellAsync),
            effectiveStateStore);
    }

    public Task InitializeAsync() => Shell.InitializeAsync();

    public async ValueTask DisposeAsync()
    {
        try
        {
            _stateStore.Save(Shell.CreateStateSnapshot());
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to persist UI shell state during shutdown.");
        }

        await _session.DisposeAsync();
    }
}
