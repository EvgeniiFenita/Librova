using Librova.UI.CoreHost;
using Librova.UI.ImportJobs;
using Librova.UI.LibraryCatalog;
using Librova.UI.Logging;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.Shell;

internal static class ShellBootstrap
{
    public static Task<ShellSession> StartDevelopmentSessionAsync(CancellationToken cancellationToken)
        => StartSessionAsync(CoreHostDevelopmentDefaults.Create(), cancellationToken);

    public static async Task<ShellSession> StartSessionAsync(
        CoreHostLaunchOptions hostOptions,
        CancellationToken cancellationToken)
    {
        UiLogging.EnsureInitialized();
        UiLogging.Information("Starting UI shell session. PipePath={PipePath}", hostOptions.PipePath);
        var process = new CoreHostProcess();

        try
        {
            await process.StartAsync(hostOptions, cancellationToken).ConfigureAwait(false);
            var importJobs = new ImportJobsService(hostOptions.PipePath);
            var libraryCatalog = new LibraryCatalogService(hostOptions.PipePath);
            UiLogging.Information("UI shell session is ready. PipePath={PipePath}", hostOptions.PipePath);
            return new ShellSession(process, hostOptions, importJobs, libraryCatalog);
        }
        catch (System.Exception error)
        {
            UiLogging.Error(error, "Failed to start UI shell session. PipePath={PipePath}", hostOptions.PipePath);
            await process.DisposeAsync().ConfigureAwait(false);
            throw;
        }
    }
}
