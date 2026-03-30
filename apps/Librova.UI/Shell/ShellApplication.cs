using Librova.UI.ViewModels;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.Shell;

internal sealed class ShellApplication : IAsyncDisposable
{
    private readonly ShellSession _session;

    public ShellApplication(ShellSession session, ShellViewModel shellViewModel)
    {
        _session = session;
        Shell = shellViewModel;
    }

    public ShellViewModel Shell { get; }

    public static async Task<ShellApplication> StartDevelopmentAsync(CancellationToken cancellationToken)
    {
        var session = await ShellBootstrap.StartDevelopmentSessionAsync(cancellationToken).ConfigureAwait(false);
        return Create(session);
    }

    public static ShellApplication Create(ShellSession session)
        => new(session, new ShellViewModel(session));

    public ValueTask DisposeAsync() => _session.DisposeAsync();
}
