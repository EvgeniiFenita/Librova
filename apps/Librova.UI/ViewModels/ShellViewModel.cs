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
        ShellLaunchOptions? launchOptions = null,
        ShellStateSnapshot? savedState = null)
    {
        _session = session;
        ImportJobs = new ImportJobsViewModel(session.ImportJobs, pathSelectionService);
        ImportJobs.WorkingDirectory = string.IsNullOrWhiteSpace(savedState?.WorkingDirectory)
            ? ImportJobsDefaults.BuildDefaultWorkingDirectory(session.HostOptions.LibraryRoot)
            : savedState.WorkingDirectory!;
        ImportJobs.AllowProbableDuplicates = savedState?.AllowProbableDuplicates ?? false;

        if (!string.IsNullOrWhiteSpace(launchOptions?.InitialSourcePath))
        {
            ImportJobs.SourcePath = launchOptions.InitialSourcePath;
        }
        else if (!string.IsNullOrWhiteSpace(savedState?.SourcePath))
        {
            ImportJobs.SourcePath = savedState.SourcePath;
        }
    }

    public string LibraryRoot => _session.HostOptions.LibraryRoot;
    public string PipePath => _session.HostOptions.PipePath;
    public ImportJobsViewModel ImportJobs { get; }

    public ShellStateSnapshot CreateStateSnapshot() => ImportJobs.CreateStateSnapshot();
}
