using Librova.UI.CoreHost;
using Librova.UI.ImportJobs;
using System;
using System.Threading.Tasks;

namespace Librova.UI.Shell;

internal sealed class ShellSession : IAsyncDisposable
{
    private readonly IAsyncDisposable _hostLifetime;

    public ShellSession(
        IAsyncDisposable hostLifetime,
        CoreHostLaunchOptions hostOptions,
        IImportJobsService importJobs)
    {
        _hostLifetime = hostLifetime;
        HostOptions = hostOptions;
        ImportJobs = importJobs;
    }

    public CoreHostLaunchOptions HostOptions { get; }
    public IImportJobsService ImportJobs { get; }

    public ValueTask DisposeAsync() => _hostLifetime.DisposeAsync();
}
