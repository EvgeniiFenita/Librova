using Librova.UI.Shell;
using Librova.UI.Mvvm;

namespace Librova.UI.ViewModels;

internal sealed class ShellViewModel : ObservableObject
{
    private readonly ShellSession _session;

    public ShellViewModel(ShellSession session)
    {
        _session = session;
        ImportJobs = new ImportJobsViewModel(session.ImportJobs);
    }

    public string LibraryRoot => _session.HostOptions.LibraryRoot;
    public string PipePath => _session.HostOptions.PipePath;
    public ImportJobsViewModel ImportJobs { get; }
}
