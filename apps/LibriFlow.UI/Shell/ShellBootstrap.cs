using LibriFlow.UI.CoreHost;
using LibriFlow.UI.ImportJobs;
using System.Threading;
using System.Threading.Tasks;

namespace LibriFlow.UI.Shell;

internal static class ShellBootstrap
{
    public static Task<ShellSession> StartDevelopmentSessionAsync(CancellationToken cancellationToken)
        => StartSessionAsync(CoreHostDevelopmentDefaults.Create(), cancellationToken);

    public static async Task<ShellSession> StartSessionAsync(
        CoreHostLaunchOptions hostOptions,
        CancellationToken cancellationToken)
    {
        var process = new CoreHostProcess();

        try
        {
            await process.StartAsync(hostOptions, cancellationToken).ConfigureAwait(false);
            var importJobs = new ImportJobsService(hostOptions.PipePath);
            return new ShellSession(process, hostOptions, importJobs);
        }
        catch
        {
            await process.DisposeAsync().ConfigureAwait(false);
            throw;
        }
    }
}
