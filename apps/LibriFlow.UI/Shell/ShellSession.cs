using LibriFlow.UI.CoreHost;
using LibriFlow.UI.ImportJobs;
using System;
using System.Threading.Tasks;

namespace LibriFlow.UI.Shell;

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
