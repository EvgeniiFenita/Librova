using Librova.UI.Desktop;
using Librova.UI.ImportJobs;
using Librova.UI.Logging;
using Librova.UI.Shell;
using Librova.UI.Mvvm;
using System;
using System.IO;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class ShellViewModel : ObservableObject
{
    private readonly ShellSession _session;
    private readonly IPathSelectionService _pathSelectionService;
    private readonly IUiPreferencesStore _preferencesStore;
    private string _preferredLibraryRoot = string.Empty;
    private string _preferencesStatusText = "Using current shell defaults.";
    private string _preferredLibraryRootValidationMessage = string.Empty;

    public ShellViewModel(
        ShellSession session,
        IPathSelectionService? pathSelectionService = null,
        ShellLaunchOptions? launchOptions = null,
        ShellStateSnapshot? savedState = null,
        IUiPreferencesStore? preferencesStore = null,
        UiPreferencesSnapshot? savedPreferences = null)
    {
        _session = session;
        _pathSelectionService = pathSelectionService ?? new NullPathSelectionService();
        _preferencesStore = preferencesStore ?? UiPreferencesStore.CreateDefault();
        ImportJobs = new ImportJobsViewModel(session.ImportJobs, pathSelectionService);
        ImportJobs.WorkingDirectory = string.IsNullOrWhiteSpace(savedState?.WorkingDirectory)
            ? ImportJobsDefaults.BuildDefaultWorkingDirectory(session.HostOptions.LibraryRoot)
            : savedState.WorkingDirectory!;
        ImportJobs.AllowProbableDuplicates = savedState?.AllowProbableDuplicates ?? false;
        _preferredLibraryRoot = savedPreferences?.PreferredLibraryRoot ?? session.HostOptions.LibraryRoot;
        UpdatePreferredLibraryRootValidation();

        if (!string.IsNullOrWhiteSpace(launchOptions?.InitialSourcePath))
        {
            ImportJobs.SourcePath = launchOptions.InitialSourcePath;
        }
        else if (!string.IsNullOrWhiteSpace(savedState?.SourcePath))
        {
            ImportJobs.SourcePath = savedState.SourcePath;
        }

        SavePreferencesCommand = new AsyncCommand(SavePreferencesAsync, CanSavePreferences);
        ResetPreferencesCommand = new AsyncCommand(ResetPreferencesAsync, () => true);
        BrowsePreferredLibraryRootCommand = new AsyncCommand(BrowsePreferredLibraryRootAsync, () => true);
    }

    public string LibraryRoot => _session.HostOptions.LibraryRoot;
    public string PipePath => _session.HostOptions.PipePath;
    public ImportJobsViewModel ImportJobs { get; }
    public AsyncCommand SavePreferencesCommand { get; }
    public AsyncCommand ResetPreferencesCommand { get; }
    public AsyncCommand BrowsePreferredLibraryRootCommand { get; }

    public string PreferredLibraryRoot
    {
        get => _preferredLibraryRoot;
        set
        {
            if (SetProperty(ref _preferredLibraryRoot, value))
            {
                UpdatePreferredLibraryRootValidation();
                SavePreferencesCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public string PreferencesStatusText
    {
        get => _preferencesStatusText;
        private set => SetProperty(ref _preferencesStatusText, value);
    }

    public string PreferredLibraryRootValidationMessage
    {
        get => _preferredLibraryRootValidationMessage;
        private set => SetProperty(ref _preferredLibraryRootValidationMessage, value);
    }

    public bool HasPreferredLibraryRootValidationError => !string.IsNullOrWhiteSpace(PreferredLibraryRootValidationMessage);
    public bool ShowPreferredLibraryRootHelperText => !HasPreferredLibraryRootValidationError;

    public ShellStateSnapshot CreateStateSnapshot() => ImportJobs.CreateStateSnapshot();

    private async Task BrowsePreferredLibraryRootAsync()
    {
        var selectedPath = await _pathSelectionService.PickWorkingDirectoryAsync(default);
        if (!string.IsNullOrWhiteSpace(selectedPath))
        {
            PreferredLibraryRoot = selectedPath;
        }
    }

    private Task SavePreferencesAsync()
    {
        _preferencesStore.Save(new UiPreferencesSnapshot
        {
            PreferredLibraryRoot = PreferredLibraryRoot
        });
        PreferencesStatusText = "Saved. Next app launch will use this library root.";
        UiLogging.Information("Saved preferred library root for next launch. PreferredLibraryRoot={PreferredLibraryRoot}", PreferredLibraryRoot);
        return Task.CompletedTask;
    }

    private Task ResetPreferencesAsync()
    {
        _preferencesStore.Clear();
        PreferredLibraryRoot = _session.HostOptions.LibraryRoot;
        PreferencesStatusText = "Reset. Next app launch will use the default library root.";
        UiLogging.Information("Cleared preferred library root override.");
        return Task.CompletedTask;
    }

    private bool CanSavePreferences() => !HasPreferredLibraryRootValidationError && !string.IsNullOrWhiteSpace(PreferredLibraryRoot);

    private void UpdatePreferredLibraryRootValidation()
    {
        PreferredLibraryRootValidationMessage = BuildPreferredLibraryRootValidationMessage(PreferredLibraryRoot);
        RaisePropertyChanged(nameof(HasPreferredLibraryRootValidationError));
        RaisePropertyChanged(nameof(ShowPreferredLibraryRootHelperText));
    }

    private static string BuildPreferredLibraryRootValidationMessage(string preferredLibraryRoot)
    {
        if (string.IsNullOrWhiteSpace(preferredLibraryRoot))
        {
            return "Library root cannot be empty.";
        }

        if (!Path.IsPathFullyQualified(preferredLibraryRoot))
        {
            return "Use an absolute library root path.";
        }

        if (File.Exists(preferredLibraryRoot))
        {
            return "Library root must not point to a file.";
        }

        return string.Empty;
    }
}
