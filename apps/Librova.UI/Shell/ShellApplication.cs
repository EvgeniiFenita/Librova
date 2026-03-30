using Librova.UI.Desktop;
using Librova.UI.ViewModels;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.Shell;

internal sealed class ShellApplication : IAsyncDisposable
{
    private readonly ShellSession _session;
    private readonly ShellStateStore _stateStore;

    public ShellApplication(ShellSession session, ShellViewModel shellViewModel, ShellStateStore stateStore)
    {
        _session = session;
        _stateStore = stateStore;
        Shell = shellViewModel;
    }

    public ShellViewModel Shell { get; }

    public static async Task<ShellApplication> StartDevelopmentAsync(CancellationToken cancellationToken)
    {
        var session = await ShellBootstrap.StartDevelopmentSessionAsync(cancellationToken).ConfigureAwait(false);
        return Create(session);
    }

    public static ShellApplication Create(
        ShellSession session,
        IPathSelectionService? pathSelectionService = null,
        ShellLaunchOptions? launchOptions = null,
        ShellStateStore? stateStore = null)
    {
        var effectiveStateStore = stateStore ?? ShellStateStore.CreateDefault();
        var savedState = effectiveStateStore.TryLoad();
        return new(
            session,
            new ShellViewModel(session, pathSelectionService, launchOptions, savedState),
            effectiveStateStore);
    }

    public async ValueTask DisposeAsync()
    {
        _stateStore.Save(Shell.CreateStateSnapshot());
        await _session.DisposeAsync();
    }
}
