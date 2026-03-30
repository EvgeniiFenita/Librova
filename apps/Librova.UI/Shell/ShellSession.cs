using Librova.UI.CoreHost;
using Librova.UI.ImportJobs;
using Librova.UI.LibraryCatalog;
using System;
using System.Threading.Tasks;

namespace Librova.UI.Shell;

internal sealed class ShellSession : IAsyncDisposable
{
    private readonly IAsyncDisposable _hostLifetime;

    public ShellSession(
        IAsyncDisposable hostLifetime,
        CoreHostLaunchOptions hostOptions,
        IImportJobsService importJobs,
        ILibraryCatalogService? libraryCatalog = null)
    {
        _hostLifetime = hostLifetime;
        HostOptions = hostOptions;
        ImportJobs = importJobs;
        LibraryCatalog = libraryCatalog ?? new NullLibraryCatalogService();
    }

    public CoreHostLaunchOptions HostOptions { get; }
    public IImportJobsService ImportJobs { get; }
    public ILibraryCatalogService LibraryCatalog { get; }

    public ValueTask DisposeAsync() => _hostLifetime.DisposeAsync();
}
