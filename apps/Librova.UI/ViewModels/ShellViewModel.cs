using Librova.UI.Desktop;
using Librova.UI.ImportJobs;
using Librova.UI.LibraryCatalog;
using Librova.UI.Logging;
using Librova.UI.Shell;
using Librova.UI.Mvvm;
using Librova.UI.Runtime;
using Librova.UI.CoreHost;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class ShellViewModel : ObservableObject, IDisposable
{
    private readonly ShellSession _session;
    private readonly IPathSelectionService _pathSelectionService;
    private readonly IUiPreferencesStore _preferencesStore;
    private readonly Func<string, UiLibraryOpenMode, Task>? _switchLibraryAsync;
    private readonly Func<Task>? _reloadShellAsync;
    private readonly ConverterValidationCoordinator _converterValidationCoordinator;
    private string _fb2CngExecutablePath = string.Empty;
    private string _converterValidationMessage = string.Empty;
    private ShellSection _currentSection = ShellSection.Library;
    private bool _isConverterProbeInProgress;
    internal Task _converterProbeTask = Task.CompletedTask;

    public ShellViewModel(
        ShellSession session,
        IPathSelectionService? pathSelectionService = null,
        ShellLaunchOptions? launchOptions = null,
        ShellStateSnapshot? savedState = null,
        IUiPreferencesStore? preferencesStore = null,
        UiPreferencesSnapshot? savedPreferences = null,
        Func<string, UiLibraryOpenMode, Task>? switchLibraryAsync = null,
        Func<Task>? reloadShellAsync = null,
        Func<string, CancellationToken, Task<Fb2ProbeResult>>? converterProbe = null)
    {
        _session = session;
        _pathSelectionService = pathSelectionService ?? new NullPathSelectionService();
        _preferencesStore = preferencesStore ?? UiPreferencesStore.CreateDefault();
        _switchLibraryAsync = switchLibraryAsync;
        _reloadShellAsync = reloadShellAsync;
        _converterValidationCoordinator = new ConverterValidationCoordinator(converterProbe);
        _converterValidationCoordinator.StateChanged += OnConverterValidationStateChanged;
        var hasConfiguredConverter = HasConfiguredBuiltInConverter(session.HostOptions);
        ImportJobs = new ImportJobsViewModel(session.ImportJobs, pathSelectionService);
        LibraryBrowser = new LibraryBrowserViewModel(
            session.LibraryCatalog,
            _pathSelectionService,
            session.HostOptions.LibraryRoot,
            hasConfiguredConverter: hasConfiguredConverter,
            initialSortKey: savedPreferences?.PreferredSortKey,
            initialSortDescending: savedPreferences?.PreferredSortDescending ?? false);
        ImportJobs.ImportCompletedSuccessfully += HandleImportCompletedSuccessfullyAsync;
        ImportJobs.PropertyChanged += OnImportJobsPropertyChanged;
        LibraryBrowser.PropertyChanged += OnLibraryBrowserPropertyChanged;
        ImportJobs.WorkingDirectory = string.IsNullOrWhiteSpace(savedState?.WorkingDirectory)
            ? ImportJobsDefaults.BuildDefaultWorkingDirectory(session.HostOptions.LibraryRoot)
            : savedState.WorkingDirectory!;
        ImportJobs.AllowProbableDuplicates = savedState?.AllowProbableDuplicates ?? false;
        ImportJobs.HasConfiguredConverter = hasConfiguredConverter;
        ImportJobs.ForceEpubConversionOnImport = hasConfiguredConverter
            && (savedPreferences?.ForceEpubConversionOnImport ?? false);
        _fb2CngExecutablePath = savedPreferences?.Fb2CngExecutablePath ?? string.Empty;

        SavePreferencesCommand = new AsyncCommand(SavePreferencesAsync, CanSavePreferences);
        BrowseFb2CngExecutablePathCommand = new AsyncCommand(BrowseFb2CngExecutablePathAsync);
        OpenLibraryCommand = new AsyncCommand(OpenLibraryAsync, () => !IsImportInProgress && _switchLibraryAsync is not null);
        CreateLibraryCommand = new AsyncCommand(CreateLibraryAsync, () => !IsImportInProgress && _switchLibraryAsync is not null);
        ShowLibrarySectionCommand = new AsyncCommand(ShowLibrarySectionAsync, () => !IsImportInProgress);
        ShowImportSectionCommand = new AsyncCommand(ShowImportSectionAsync, () => !IsImportInProgress);
        ShowSettingsSectionCommand = new AsyncCommand(ShowSettingsSectionAsync, () => !IsImportInProgress);

        _converterValidationCoordinator.Initialize(_fb2CngExecutablePath);

        if (session.HostOptions.LibraryOpenMode == UiLibraryOpenMode.CreateNew)
        {
            _currentSection = ShellSection.Import;
        }

        if (launchOptions?.InitialSourcePaths is { Length: > 0 } launchSourcePaths)
        {
            ImportJobs.ApplyDroppedSourcePaths(launchSourcePaths);
        }
        else if (savedState?.SourcePaths is { Length: > 0 } savedSourcePaths)
        {
            ImportJobs.ApplyDroppedSourcePaths(savedSourcePaths);
        }
    }

    public string LibraryRoot => _session.HostOptions.LibraryRoot;
    public string CurrentLibraryStatisticsText => LibraryBrowser.LibraryStatisticsText;
    public string PipePath => _session.HostOptions.PipePath;
    public ImportJobsViewModel ImportJobs { get; }
    public LibraryBrowserViewModel LibraryBrowser { get; }
    public AsyncCommand SavePreferencesCommand { get; }
    public AsyncCommand BrowseFb2CngExecutablePathCommand { get; }
    public AsyncCommand OpenLibraryCommand { get; }
    public AsyncCommand CreateLibraryCommand { get; }
    public AsyncCommand ShowLibrarySectionCommand { get; }
    public AsyncCommand ShowImportSectionCommand { get; }
    public AsyncCommand ShowSettingsSectionCommand { get; }

    public string Fb2CngExecutablePath
    {
        get => _fb2CngExecutablePath;
        set
        {
            if (SetProperty(ref _fb2CngExecutablePath, value))
            {
                _converterValidationCoordinator.ScheduleValidation(_fb2CngExecutablePath);
            }
        }
    }

    public ShellSection CurrentSection
    {
        get => _currentSection;
        private set
        {
            if (SetProperty(ref _currentSection, value))
            {
                RaisePropertyChanged(nameof(IsLibrarySectionActive));
                RaisePropertyChanged(nameof(IsImportSectionActive));
                RaisePropertyChanged(nameof(IsSettingsSectionActive));
                RaisePropertyChanged(nameof(CurrentSectionTitle));
                RaisePropertyChanged(nameof(CurrentSectionDescription));
            }
        }
    }

    public string ConverterValidationMessage
    {
        get => _converterValidationMessage;
        private set => SetProperty(ref _converterValidationMessage, value);
    }

    public bool HasConverterValidationError => !string.IsNullOrWhiteSpace(ConverterValidationMessage);
    public bool ShowConverterHelperText => !HasConverterValidationError && !_isConverterProbeInProgress;
    public bool IsConverterProbeInProgress => _isConverterProbeInProgress;
    public string ConverterProbeStatusText => _isConverterProbeInProgress ? "Validating converter…" : string.Empty;
    public bool IsLibrarySectionActive => CurrentSection is ShellSection.Library;
    public bool IsImportSectionActive => CurrentSection is ShellSection.Import;
    public bool IsSettingsSectionActive => CurrentSection is ShellSection.Settings;
    public bool IsImportInProgress => ImportJobs.IsBusy;
    public bool CanSwitchLibrary => !IsImportInProgress && _switchLibraryAsync is not null;
    public bool HasConfiguredConverter => !string.IsNullOrWhiteSpace(Fb2CngExecutablePath);
    public string CurrentSectionTitle => CurrentSection switch
    {
        ShellSection.Library => "Library",
        ShellSection.Import => "Import",
        ShellSection.Settings => "Settings",
        _ => "Librova"
    };
    public string CurrentSectionDescription => CurrentSection switch
    {
        ShellSection.Library => "Browse the managed library as a visual grid, inspect book metadata, export books, and move books to Recycle Bin.",
        ShellSection.Import => "Bring EPUB, FB2, and ZIP sources into the managed library through the native import pipeline.",
        ShellSection.Settings => "Adjust converter preferences and inspect runtime diagnostics.",
        _ => string.Empty
    };
    public string UiLogFilePath => RuntimeEnvironment.GetUiLogFilePathForLibrary(LibraryRoot);
    public string UiStateFilePath => ShellStateStore.CreateDefault().FilePath;
    public string UiPreferencesFilePath => UiPreferencesStore.CreateDefault().FilePath;
    public string HostExecutablePath => _session.HostOptions.ExecutablePath;
    public string HostLogFilePath => Path.Combine(LibraryRoot, "Logs", "host.log");
    public string ApplicationVersionText => $"Version {ApplicationVersion.Value}";
    public string ApplicationAuthorText => ApplicationVersion.Author;
    public string ApplicationContactEmailText => ApplicationVersion.ContactEmail;
    public string DiagnosticsHintText =>
        "If startup or import flow looks suspicious, inspect the UI log first, then the host log.";
    public string ConverterHintText =>
        "Librova uses the built-in fb2cng / fbc.exe converter profile. Leave the path empty to keep EPUB conversion disabled.";

    public ShellStateSnapshot CreateStateSnapshot() => ImportJobs.CreateStateSnapshot();

    public Task InitializeAsync() => LibraryBrowser.RefreshAsync();

    public Task ActivateImportSectionAsync()
    {
        CurrentSection = ShellSection.Import;
        return Task.CompletedTask;
    }

    public Task ActivateSectionAsync(ShellSection section)
    {
        CurrentSection = section;
        return Task.CompletedTask;
    }

    private Task ShowLibrarySectionAsync()
    {
        CurrentSection = ShellSection.Library;
        return Task.CompletedTask;
    }

    private Task ShowImportSectionAsync()
    {
        CurrentSection = ShellSection.Import;
        return Task.CompletedTask;
    }

    private Task ShowSettingsSectionAsync()
    {
        CurrentSection = ShellSection.Settings;
        return Task.CompletedTask;
    }

    private async Task BrowseFb2CngExecutablePathAsync()
    {
        var selectedPath = await _pathSelectionService.PickExecutableFileAsync("Select fb2cng executable", default);
        if (!string.IsNullOrWhiteSpace(selectedPath))
        {
            Fb2CngExecutablePath = selectedPath;
        }
    }

    private async Task OpenLibraryAsync()
    {
        var selectedPath = await _pathSelectionService.PickWorkingDirectoryAsync(default);
        if (string.IsNullOrWhiteSpace(selectedPath) || _switchLibraryAsync is null)
        {
            return;
        }

        var validationMessage = LibraryRootInspection.BuildOpenExistingValidationMessage(selectedPath);
        if (!string.IsNullOrWhiteSpace(validationMessage))
        {
            UiLogging.Warning(
                "Rejected Open Library request. LibraryRoot={LibraryRoot} Validation={Validation}",
                selectedPath,
                validationMessage);
            throw new InvalidOperationException(validationMessage);
        }

        await _switchLibraryAsync(selectedPath, UiLibraryOpenMode.OpenExisting);
    }

    private async Task CreateLibraryAsync()
    {
        var selectedPath = await _pathSelectionService.PickWorkingDirectoryAsync(default);
        if (string.IsNullOrWhiteSpace(selectedPath) || _switchLibraryAsync is null)
        {
            return;
        }

        var validationMessage = LibraryRootInspection.BuildCreateNewValidationMessage(selectedPath);
        if (!string.IsNullOrWhiteSpace(validationMessage))
        {
            UiLogging.Warning(
                "Rejected Create Library request. LibraryRoot={LibraryRoot} Validation={Validation}",
                selectedPath,
                validationMessage);
            throw new InvalidOperationException(validationMessage);
        }

        await _switchLibraryAsync(selectedPath, UiLibraryOpenMode.CreateNew);
    }

    private Task SavePreferencesAsync()
    {
        var hasConfiguredConverter = !string.IsNullOrWhiteSpace(Fb2CngExecutablePath);
        var current = _preferencesStore.TryLoad();
        _preferencesStore.Save(BuildPreferencesSnapshot(new UiPreferencesSnapshot
        {
            PreferredLibraryRoot = _session.HostOptions.LibraryRoot,
            ConverterMode = hasConfiguredConverter ? UiConverterMode.BuiltInFb2Cng : UiConverterMode.Disabled,
            Fb2CngExecutablePath = hasConfiguredConverter ? Fb2CngExecutablePath : null,
            Fb2CngConfigPath = hasConfiguredConverter ? current?.Fb2CngConfigPath : null,
            ForceEpubConversionOnImport = hasConfiguredConverter && ImportJobs.ForceEpubConversionOnImport,
            PreferredSortKey = LibraryBrowser.SelectedSortKey?.Key,
            PreferredSortDescending = LibraryBrowser.SortDescending
        }));
        UiLogging.Information("Saved converter preferences for current library. LibraryRoot={LibraryRoot}", LibraryRoot);
        return _reloadShellAsync is null ? Task.CompletedTask : _reloadShellAsync();
    }

    private bool CanSavePreferences() =>
        !HasConverterValidationError && !_isConverterProbeInProgress;

    private void RaiseProbeProperties()
    {
        RaisePropertyChanged(nameof(HasConverterValidationError));
        RaisePropertyChanged(nameof(ShowConverterHelperText));
        RaisePropertyChanged(nameof(IsConverterProbeInProgress));
        RaisePropertyChanged(nameof(ConverterProbeStatusText));
        SavePreferencesCommand.RaiseCanExecuteChanged();
    }

    private static bool HasConfiguredBuiltInConverter(CoreHostLaunchOptions hostOptions) =>
        hostOptions.ConverterMode is UiConverterMode.BuiltInFb2Cng
        && !string.IsNullOrWhiteSpace(hostOptions.Fb2CngExecutablePath);

    private void OnConverterValidationStateChanged()
    {
        _converterProbeTask = _converterValidationCoordinator.CurrentProbeTask;
        _isConverterProbeInProgress = _converterValidationCoordinator.IsProbeInProgress;
        ConverterValidationMessage = _converterValidationCoordinator.ValidationMessage;
        RaiseProbeProperties();
    }

    private async Task HandleImportCompletedSuccessfullyAsync(ImportJobResultModel result)
    {
        await LibraryBrowser.RefreshAsync();
    }

    private void OnImportJobsPropertyChanged(object? sender, PropertyChangedEventArgs eventArgs)
    {
        if (eventArgs.PropertyName is not nameof(ImportJobsViewModel.IsBusy))
        {
            return;
        }

        RaisePropertyChanged(nameof(IsImportInProgress));
        RaisePropertyChanged(nameof(CanSwitchLibrary));
        if (ImportJobs.IsBusy && CurrentSection is not ShellSection.Import)
        {
            CurrentSection = ShellSection.Import;
        }

        OpenLibraryCommand.RaiseCanExecuteChanged();
        CreateLibraryCommand.RaiseCanExecuteChanged();
        ShowLibrarySectionCommand.RaiseCanExecuteChanged();
        ShowImportSectionCommand.RaiseCanExecuteChanged();
        ShowSettingsSectionCommand.RaiseCanExecuteChanged();
    }

    private void OnLibraryBrowserPropertyChanged(object? sender, PropertyChangedEventArgs eventArgs)
    {
        if (eventArgs.PropertyName is nameof(LibraryBrowserViewModel.LibraryStatisticsText))
        {
            RaisePropertyChanged(nameof(CurrentLibraryStatisticsText));
        }

        if (eventArgs.PropertyName is nameof(LibraryBrowserViewModel.SelectedSortKey)
            && LibraryBrowser.SelectedSortKey is not null)
        {
            SaveSortPreference(LibraryBrowser.SelectedSortKey.Key, LibraryBrowser.SortDescending);
        }

        if (eventArgs.PropertyName is nameof(LibraryBrowserViewModel.SortDescending)
            && LibraryBrowser.SelectedSortKey is not null)
        {
            SaveSortPreference(LibraryBrowser.SelectedSortKey.Key, LibraryBrowser.SortDescending);
        }
    }

    private void SaveSortPreference(BookSortModel sortKey, bool sortDescending)
    {
        try
        {
            var current = _preferencesStore.TryLoad();
            _preferencesStore.Save(BuildPreferencesSnapshot(new UiPreferencesSnapshot
            {
                PreferredLibraryRoot = _session.HostOptions.LibraryRoot,
                ConverterMode = current?.ConverterMode ?? UiConverterMode.Disabled,
                PortablePreferredLibraryRoot = current?.PortablePreferredLibraryRoot,
                Fb2CngExecutablePath = current?.Fb2CngExecutablePath,
                Fb2CngConfigPath = current?.Fb2CngConfigPath,
                ForceEpubConversionOnImport = current?.ForceEpubConversionOnImport ?? false,
                PreferredSortKey = sortKey,
                PreferredSortDescending = sortDescending
            }));
        }
        catch (Exception error)
        {
            UiLogging.Warning("Failed to persist sort preference. {Error}", error.Message);
        }
    }

    private UiPreferencesSnapshot BuildPreferencesSnapshot(UiPreferencesSnapshot snapshot)
    {
        var libraryRoot = snapshot.PreferredLibraryRoot ?? _session.HostOptions.LibraryRoot;
        return new UiPreferencesSnapshot
        {
            PreferredLibraryRoot = libraryRoot,
            PortablePreferredLibraryRoot = RuntimeEnvironment.BuildPortableLibraryRootPreference(libraryRoot),
            ConverterMode = snapshot.ConverterMode,
            Fb2CngExecutablePath = snapshot.Fb2CngExecutablePath,
            Fb2CngConfigPath = snapshot.Fb2CngConfigPath,
            ForceEpubConversionOnImport = snapshot.ForceEpubConversionOnImport,
            PreferredSortKey = snapshot.PreferredSortKey,
            PreferredSortDescending = snapshot.PreferredSortDescending
        };
    }

    public void Dispose()
    {
        ImportJobs.ImportCompletedSuccessfully -= HandleImportCompletedSuccessfullyAsync;
        ImportJobs.PropertyChanged -= OnImportJobsPropertyChanged;
        LibraryBrowser.PropertyChanged -= OnLibraryBrowserPropertyChanged;
        LibraryBrowser.Dispose();
        _converterValidationCoordinator.StateChanged -= OnConverterValidationStateChanged;
        _converterValidationCoordinator.Dispose();
        _converterProbeTask = Task.CompletedTask;
        _isConverterProbeInProgress = false;
        ConverterValidationMessage = string.Empty;
        RaiseProbeProperties();
    }
}
