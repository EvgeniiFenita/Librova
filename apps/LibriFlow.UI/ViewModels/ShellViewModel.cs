using LibriFlow.UI.Shell;
using LibriFlow.UI.Mvvm;

namespace LibriFlow.UI.ViewModels;

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
