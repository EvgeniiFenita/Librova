using Librova.UI.Desktop;
using Librova.UI.ImportJobs;
using Librova.UI.Shell;
using Librova.UI.Mvvm;

namespace Librova.UI.ViewModels;

internal sealed class ShellViewModel : ObservableObject
{
    private readonly ShellSession _session;

    public ShellViewModel(
        ShellSession session,
        IPathSelectionService? pathSelectionService = null,
        ShellLaunchOptions? launchOptions = null)
    {
        _session = session;
        ImportJobs = new ImportJobsViewModel(session.ImportJobs, pathSelectionService);
        ImportJobs.WorkingDirectory = ImportJobsDefaults.BuildDefaultWorkingDirectory(session.HostOptions.LibraryRoot);

        if (!string.IsNullOrWhiteSpace(launchOptions?.InitialSourcePath))
        {
            ImportJobs.SourcePath = launchOptions.InitialSourcePath;
        }
    }

    public string LibraryRoot => _session.HostOptions.LibraryRoot;
    public string PipePath => _session.HostOptions.PipePath;
    public ImportJobsViewModel ImportJobs { get; }
}
