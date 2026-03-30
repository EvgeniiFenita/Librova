using Librova.UI.CoreHost;
using Librova.UI.ImportJobs;
using System;
using System.Threading.Tasks;

namespace Librova.UI.Shell;

internal sealed class ShellSession : IAsyncDisposable
{
    private readonly CoreHostProcess _coreHostProcess;

    public ShellSession(
        CoreHostProcess coreHostProcess,
        CoreHostLaunchOptions hostOptions,
        IImportJobsService importJobs)
    {
        _coreHostProcess = coreHostProcess;
        HostOptions = hostOptions;
        ImportJobs = importJobs;
    }

    public CoreHostLaunchOptions HostOptions { get; }
    public IImportJobsService ImportJobs { get; }

    public ValueTask DisposeAsync() => _coreHostProcess.DisposeAsync();
}
