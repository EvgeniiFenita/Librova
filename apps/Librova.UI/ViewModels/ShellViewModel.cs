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
    private readonly ShellConverterSettingsState _converterSettings = new();
    private readonly ShellSectionState _sectionState = new();
    private readonly ShellPreferencesPersistence _preferencesPersistence;
    private readonly ShellLibrarySwitchController _librarySwitchController;
    private readonly ShellImportWorkflowController _importWorkflowController;
    private readonly ShellConverterValidationWorkflow _converterValidationWorkflow;
    private readonly ShellStartupStateApplier _startupStateApplier;
    private readonly ShellConverterPathController _converterPathController;
    private readonly string _uiStateFilePath;
    private readonly string _uiPreferencesFilePath;
    private Task _converterProbeTask = Task.CompletedTask;

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
        _preferencesPersistence = new ShellPreferencesPersistence(_preferencesStore, session.HostOptions.LibraryRoot, _reloadShellAsync);
        _librarySwitchController = new ShellLibrarySwitchController(_pathSelectionService, session.HostOptions.LibraryRoot, _switchLibraryAsync);
        _converterValidationCoordinator = new ConverterValidationCoordinator(converterProbe);
        _uiStateFilePath = ShellStateStore.CreateDefault().FilePath;
        _uiPreferencesFilePath = _preferencesStore is UiPreferencesStore concretePreferencesStore
            ? concretePreferencesStore.FilePath
            : RuntimeEnvironment.GetDefaultUiPreferencesFilePath();
        ImportJobs = new ImportJobsViewModel(session.ImportJobs, pathSelectionService);
        _startupStateApplier = new ShellStartupStateApplier(
            session.HostOptions,
            launchOptions,
            savedState,
            savedPreferences,
            ImportJobs,
            _converterSettings,
            section => _sectionState.CurrentSection = section);
        var hasConfiguredConverter = _startupStateApplier.Apply();
        LibraryBrowser = new LibraryBrowserViewModel(
            session.LibraryCatalog,
            _pathSelectionService,
            session.HostOptions.LibraryRoot,
            hasConfiguredConverter: hasConfiguredConverter,
            initialSortKey: savedPreferences?.PreferredSortKey,
            initialSortDescending: savedPreferences?.PreferredSortDescending ?? false,
            navigateToImport: () => CurrentSection = ShellSection.Import);
        _importWorkflowController = new ShellImportWorkflowController(
            ImportJobs,
            LibraryBrowser,
            () => CurrentSection,
            section => CurrentSection = section,
            RaiseImportStateProperties,
            RaiseNavigationCanExecuteChanged,
            () => RaisePropertyChanged(nameof(CurrentLibraryStatisticsText)),
            SaveSortPreference);
        _converterValidationWorkflow = new ShellConverterValidationWorkflow(
            _converterValidationCoordinator,
            _converterSettings,
            SavePreferencesAsync,
            task => _converterProbeTask = task,
            message => ConverterValidationMessage = message,
            RaiseProbeProperties);
        _converterPathController = new ShellConverterPathController(_pathSelectionService, value => Fb2CngExecutablePath = value);
        ImportJobs.ImportCompletedSuccessfully += _importWorkflowController.HandleImportCompletedSuccessfullyAsync;
        ImportJobs.PropertyChanged += _importWorkflowController.HandleImportJobsPropertyChanged;
        LibraryBrowser.PropertyChanged += _importWorkflowController.HandleLibraryBrowserPropertyChanged;

        SavePreferencesCommand = new AsyncCommand(SavePreferencesAsync, CanSavePreferences);
        BrowseFb2CngExecutablePathCommand = new AsyncCommand(BrowseFb2CngExecutablePathAsync);
        ClearConverterPathCommand = new AsyncCommand(ClearConverterPathAsync);
        OpenLibraryCommand = new AsyncCommand(OpenLibraryAsync, () => !IsImportInProgress && _switchLibraryAsync is not null);
        CreateLibraryCommand = new AsyncCommand(CreateLibraryAsync, () => !IsImportInProgress && _switchLibraryAsync is not null);
        ShowLibrarySectionCommand = new AsyncCommand(ShowLibrarySectionAsync, () => !IsImportInProgress);
        ShowImportSectionCommand = new AsyncCommand(ShowImportSectionAsync, () => !IsImportInProgress);
        ShowSettingsSectionCommand = new AsyncCommand(ShowSettingsSectionAsync, () => !IsImportInProgress);

        _converterValidationCoordinator.Initialize(_converterSettings.Fb2CngExecutablePath);
        _converterValidationWorkflow.ApplyState(allowAutoSave: false);
        _converterValidationCoordinator.StateChanged += OnConverterValidationStateChanged;
    }

    public string LibraryRoot => _session.HostOptions.LibraryRoot;
    public string CurrentLibraryStatisticsText => LibraryBrowser.LibraryStatisticsText;
    public string PipePath => _session.HostOptions.PipePath;
    public ImportJobsViewModel ImportJobs { get; }
    public LibraryBrowserViewModel LibraryBrowser { get; }
    public AsyncCommand SavePreferencesCommand { get; }
    public AsyncCommand BrowseFb2CngExecutablePathCommand { get; }
    public AsyncCommand ClearConverterPathCommand { get; }
    public AsyncCommand OpenLibraryCommand { get; }
    public AsyncCommand CreateLibraryCommand { get; }
    public AsyncCommand ShowLibrarySectionCommand { get; }
    public AsyncCommand ShowImportSectionCommand { get; }
    public AsyncCommand ShowSettingsSectionCommand { get; }

    public string Fb2CngExecutablePath
    {
        get => _converterSettings.Fb2CngExecutablePath;
        set
        {
            var previousValue = _converterSettings.Fb2CngExecutablePath;
            _converterSettings.Fb2CngExecutablePath = value;
            if (SetProperty(ref previousValue, _converterSettings.Fb2CngExecutablePath))
            {
                _converterValidationCoordinator.ScheduleValidation(_converterSettings.Fb2CngExecutablePath);
                RaisePropertyChanged(nameof(HasConfiguredConverter));
                RaisePropertyChanged(nameof(IsConverterConfiguredSuccessfully));
            }
        }
    }

    public ShellSection CurrentSection
    {
        get => _sectionState.CurrentSection;
        private set
        {
            var previousValue = _sectionState.CurrentSection;
            _sectionState.CurrentSection = value;
            if (SetProperty(ref previousValue, _sectionState.CurrentSection))
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
        get => _converterSettings.ConverterValidationMessage;
        private set
        {
            var previousValue = _converterSettings.ConverterValidationMessage;
            _converterSettings.ConverterValidationMessage = value;
            SetProperty(ref previousValue, _converterSettings.ConverterValidationMessage);
        }
    }

    public bool HasConverterValidationError => _converterSettings.HasConverterValidationError;
    public bool ShowConverterHelperText => _converterSettings.ShowConverterHelperText;
    public bool IsConverterProbeInProgress => _converterSettings.IsConverterProbeInProgress;
    public string ConverterProbeStatusText => _converterSettings.ConverterProbeStatusText;
    public bool IsConverterConfiguredSuccessfully => _converterSettings.IsConverterConfiguredSuccessfully;
    public bool IsLibrarySectionActive => _sectionState.IsLibrarySectionActive;
    public bool IsImportSectionActive => _sectionState.IsImportSectionActive;
    public bool IsSettingsSectionActive => _sectionState.IsSettingsSectionActive;
    public bool IsImportInProgress => ImportJobs.IsBusy;
    public bool CanSwitchLibrary => !IsImportInProgress && _switchLibraryAsync is not null;
    public bool HasConfiguredConverter => _converterSettings.HasConfiguredConverter;
    public string CurrentSectionTitle => _sectionState.CurrentSectionTitle;
    public string CurrentSectionDescription => _sectionState.CurrentSectionDescription;
    public string UiLogFilePath => RuntimeEnvironment.GetUiLogFilePathForLibrary(LibraryRoot);
    public string UiStateFilePath => _uiStateFilePath;
    public string UiPreferencesFilePath => _uiPreferencesFilePath;
    public string HostExecutablePath => _session.HostOptions.ExecutablePath;
    public string HostLogFilePath => Path.Combine(LibraryRoot, "Logs", "host.log");
    public string ApplicationVersionText => $"Version {ApplicationVersion.Value}";
    public string ApplicationAuthorText => ApplicationVersion.Author;
    public string ApplicationContactEmailText => ApplicationVersion.ContactEmail;
    public string DiagnosticsHintText =>
        "If startup or import flow looks suspicious, inspect the UI log first, then the host log.";
    public string ConverterHintText =>
        "Settings are applied automatically after validation. Use × to remove the configured converter.";

    public ShellStateSnapshot CreateStateSnapshot() => ImportJobs.CreateStateSnapshot();

    public Task InitializeAsync() => LibraryBrowser.RefreshAsync();

    public Task WaitForConverterProbeAsync() => _converterProbeTask;

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

    private Task ClearConverterPathAsync() => _converterPathController.ClearConverterPathAsync();

    private Task BrowseFb2CngExecutablePathAsync() => _converterPathController.BrowseFb2CngExecutablePathAsync();

    private Task OpenLibraryAsync() => _librarySwitchController.OpenLibraryAsync();

    private Task CreateLibraryAsync() => _librarySwitchController.CreateLibraryAsync();

    private Task SavePreferencesAsync()
        => _preferencesPersistence.SaveConverterPreferencesAsync(
            Fb2CngExecutablePath,
            ImportJobs.ForceEpubConversionOnImport,
            LibraryBrowser.SelectedSortKey?.Key,
            LibraryBrowser.SortDescending);

    private bool CanSavePreferences() => _converterValidationWorkflow.CanSavePreferences();

    private void RaiseProbeProperties()
    {
        RaisePropertyChanged(nameof(HasConverterValidationError));
        RaisePropertyChanged(nameof(ShowConverterHelperText));
        RaisePropertyChanged(nameof(IsConverterProbeInProgress));
        RaisePropertyChanged(nameof(ConverterProbeStatusText));
        RaisePropertyChanged(nameof(IsConverterConfiguredSuccessfully));
        SavePreferencesCommand.RaiseCanExecuteChanged();
    }

    private void RaiseImportStateProperties()
    {
        RaisePropertyChanged(nameof(IsImportInProgress));
        RaisePropertyChanged(nameof(CanSwitchLibrary));
    }

    private void RaiseNavigationCanExecuteChanged()
    {
        OpenLibraryCommand.RaiseCanExecuteChanged();
        CreateLibraryCommand.RaiseCanExecuteChanged();
        ShowLibrarySectionCommand.RaiseCanExecuteChanged();
        ShowImportSectionCommand.RaiseCanExecuteChanged();
        ShowSettingsSectionCommand.RaiseCanExecuteChanged();
    }

    private void OnConverterValidationStateChanged()
        => _converterValidationWorkflow.ApplyState(allowAutoSave: true);

    private void SaveSortPreference(BookSortModel sortKey, bool sortDescending)
        => _preferencesPersistence.TrySaveSortPreference(sortKey, sortDescending);

    public void Dispose()
    {
        ImportJobs.ImportCompletedSuccessfully -= _importWorkflowController.HandleImportCompletedSuccessfullyAsync;
        ImportJobs.PropertyChanged -= _importWorkflowController.HandleImportJobsPropertyChanged;
        LibraryBrowser.PropertyChanged -= _importWorkflowController.HandleLibraryBrowserPropertyChanged;
        ImportJobs.Dispose();
        LibraryBrowser.Dispose();
        _converterValidationCoordinator.StateChanged -= OnConverterValidationStateChanged;
        _converterValidationCoordinator.Dispose();
        _converterValidationWorkflow.Reset();
    }
}
