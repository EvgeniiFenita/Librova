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
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class ShellViewModel : ObservableObject
{
    private readonly ShellSession _session;
    private readonly IPathSelectionService _pathSelectionService;
    private readonly IUiPreferencesStore _preferencesStore;
    private readonly Func<string, Task>? _switchLibraryAsync;
    private UiConverterMode _selectedConverterMode;
    private string _fb2CngExecutablePath = string.Empty;
    private string _fb2CngConfigPath = string.Empty;
    private string _customConverterExecutablePath = string.Empty;
    private string _customConverterArgumentsText = string.Empty;
    private UiConverterOutputMode _selectedCustomConverterOutputMode = UiConverterOutputMode.ExactDestinationPath;
    private string _converterValidationMessage = string.Empty;
    private ShellSection _currentSection = ShellSection.Library;

    public ShellViewModel(
        ShellSession session,
        IPathSelectionService? pathSelectionService = null,
        ShellLaunchOptions? launchOptions = null,
        ShellStateSnapshot? savedState = null,
        IUiPreferencesStore? preferencesStore = null,
        UiPreferencesSnapshot? savedPreferences = null,
        Func<string, Task>? switchLibraryAsync = null)
    {
        _session = session;
        _pathSelectionService = pathSelectionService ?? new NullPathSelectionService();
        _preferencesStore = preferencesStore ?? UiPreferencesStore.CreateDefault();
        _switchLibraryAsync = switchLibraryAsync;
        ImportJobs = new ImportJobsViewModel(session.ImportJobs, pathSelectionService);
        LibraryBrowser = new LibraryBrowserViewModel(session.LibraryCatalog, _pathSelectionService, session.HostOptions.LibraryRoot);
        ImportJobs.ImportCompletedSuccessfully += HandleImportCompletedSuccessfullyAsync;
        ImportJobs.PropertyChanged += OnImportJobsPropertyChanged;
        ImportJobs.WorkingDirectory = string.IsNullOrWhiteSpace(savedState?.WorkingDirectory)
            ? ImportJobsDefaults.BuildDefaultWorkingDirectory(session.HostOptions.LibraryRoot)
            : savedState.WorkingDirectory!;
        ImportJobs.AllowProbableDuplicates = savedState?.AllowProbableDuplicates ?? false;
        _selectedConverterMode = savedPreferences?.ConverterMode ?? UiConverterMode.Disabled;
        _fb2CngExecutablePath = savedPreferences?.Fb2CngExecutablePath ?? string.Empty;
        _fb2CngConfigPath = savedPreferences?.Fb2CngConfigPath ?? string.Empty;
        _customConverterExecutablePath = savedPreferences?.CustomConverterExecutablePath ?? string.Empty;
        _customConverterArgumentsText = savedPreferences?.CustomConverterArguments is { Length: > 0 } arguments
            ? string.Join(Environment.NewLine, arguments)
            : string.Empty;
        _selectedCustomConverterOutputMode = savedPreferences?.CustomConverterOutputMode ?? UiConverterOutputMode.ExactDestinationPath;

        SavePreferencesCommand = new AsyncCommand(SavePreferencesAsync, CanSavePreferences);
        OpenLibraryCommand = new AsyncCommand(OpenLibraryAsync, () => !IsImportInProgress && _switchLibraryAsync is not null);
        CreateLibraryCommand = new AsyncCommand(CreateLibraryAsync, () => !IsImportInProgress && _switchLibraryAsync is not null);
        ShowLibrarySectionCommand = new AsyncCommand(ShowLibrarySectionAsync, () => !IsImportInProgress && CurrentSection is not ShellSection.Library);
        ShowImportSectionCommand = new AsyncCommand(ShowImportSectionAsync, () => !IsImportInProgress && CurrentSection is not ShellSection.Import);
        ShowSettingsSectionCommand = new AsyncCommand(ShowSettingsSectionAsync, () => !IsImportInProgress && CurrentSection is not ShellSection.Settings);

        UpdateConverterValidation();

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
    public LibraryBrowserViewModel LibraryBrowser { get; }
    public AsyncCommand SavePreferencesCommand { get; }
    public AsyncCommand OpenLibraryCommand { get; }
    public AsyncCommand CreateLibraryCommand { get; }
    public AsyncCommand ShowLibrarySectionCommand { get; }
    public AsyncCommand ShowImportSectionCommand { get; }
    public AsyncCommand ShowSettingsSectionCommand { get; }

    public UiConverterMode SelectedConverterMode
    {
        get => _selectedConverterMode;
        set
        {
            if (SetProperty(ref _selectedConverterMode, value))
            {
                UpdateConverterValidation();
                RaiseConverterPropertiesChanged();
            }
        }
    }

    public string Fb2CngExecutablePath
    {
        get => _fb2CngExecutablePath;
        set
        {
            if (SetProperty(ref _fb2CngExecutablePath, value))
            {
                UpdateConverterValidation();
                SavePreferencesCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public string Fb2CngConfigPath
    {
        get => _fb2CngConfigPath;
        set
        {
            if (SetProperty(ref _fb2CngConfigPath, value))
            {
                UpdateConverterValidation();
                SavePreferencesCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public string CustomConverterExecutablePath
    {
        get => _customConverterExecutablePath;
        set
        {
            if (SetProperty(ref _customConverterExecutablePath, value))
            {
                UpdateConverterValidation();
                SavePreferencesCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public string CustomConverterArgumentsText
    {
        get => _customConverterArgumentsText;
        set
        {
            if (SetProperty(ref _customConverterArgumentsText, value))
            {
                UpdateConverterValidation();
                SavePreferencesCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public UiConverterOutputMode SelectedCustomConverterOutputMode
    {
        get => _selectedCustomConverterOutputMode;
        set
        {
            if (SetProperty(ref _selectedCustomConverterOutputMode, value))
            {
                UpdateConverterValidation();
                SavePreferencesCommand.RaiseCanExecuteChanged();
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
                ShowLibrarySectionCommand.RaiseCanExecuteChanged();
                ShowImportSectionCommand.RaiseCanExecuteChanged();
                ShowSettingsSectionCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public string ConverterValidationMessage
    {
        get => _converterValidationMessage;
        private set => SetProperty(ref _converterValidationMessage, value);
    }

    public bool HasConverterValidationError => !string.IsNullOrWhiteSpace(ConverterValidationMessage);
    public bool ShowConverterHelperText => !HasConverterValidationError;
    public bool IsLibrarySectionActive => CurrentSection is ShellSection.Library;
    public bool IsImportSectionActive => CurrentSection is ShellSection.Import;
    public bool IsSettingsSectionActive => CurrentSection is ShellSection.Settings;
    public bool IsImportInProgress => ImportJobs.IsBusy;
    public bool CanSwitchLibrary => !IsImportInProgress && _switchLibraryAsync is not null;
    public bool IsConverterDisabled => SelectedConverterMode is UiConverterMode.Disabled;
    public bool IsBuiltInConverterSelected => SelectedConverterMode is UiConverterMode.BuiltInFb2Cng;
    public bool IsCustomConverterSelected => SelectedConverterMode is UiConverterMode.CustomCommand;
    public IReadOnlyList<UiConverterMode> AvailableConverterModes { get; } = Enum.GetValues<UiConverterMode>();
    public IReadOnlyList<UiConverterOutputMode> AvailableConverterOutputModes { get; } = Enum.GetValues<UiConverterOutputMode>();
    public string CurrentSectionTitle => CurrentSection switch
    {
        ShellSection.Library => "Library",
        ShellSection.Import => "Import",
        ShellSection.Settings => "Settings",
        _ => "Librova"
    };
    public string CurrentSectionDescription => CurrentSection switch
    {
        ShellSection.Library => "Browse the managed library as a visual grid, inspect book metadata, export books, and move books to trash.",
        ShellSection.Import => "Bring EPUB, FB2, and ZIP sources into the managed library through the native import pipeline.",
        ShellSection.Settings => "Adjust converter preferences and inspect runtime diagnostics.",
        _ => string.Empty
    };
    public string UiLogFilePath => RuntimeEnvironment.GetUiLogFilePathForLibrary(LibraryRoot);
    public string UiStateFilePath => ShellStateStore.CreateDefault().FilePath;
    public string UiPreferencesFilePath => UiPreferencesStore.CreateDefault().FilePath;
    public string HostExecutablePath => _session.HostOptions.ExecutablePath;
    public string HostLogFilePath => Path.Combine(LibraryRoot, "Logs", "host.log");
    public string DiagnosticsHintText =>
        "If startup or import flow looks suspicious, inspect the UI log first, then the host log.";

    public ShellStateSnapshot CreateStateSnapshot() => ImportJobs.CreateStateSnapshot();

    public Task InitializeAsync() => LibraryBrowser.RefreshAsync();

    public Task ActivateImportSectionAsync()
    {
        CurrentSection = ShellSection.Import;
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

    private async Task OpenLibraryAsync()
    {
        var selectedPath = await _pathSelectionService.PickWorkingDirectoryAsync(default);
        if (!string.IsNullOrWhiteSpace(selectedPath) && _switchLibraryAsync is not null)
        {
            await _switchLibraryAsync(selectedPath);
        }
    }

    private async Task CreateLibraryAsync()
    {
        var selectedPath = await _pathSelectionService.PickWorkingDirectoryAsync(default);
        if (!string.IsNullOrWhiteSpace(selectedPath) && _switchLibraryAsync is not null)
        {
            await _switchLibraryAsync(selectedPath);
        }
    }

    private Task SavePreferencesAsync()
    {
        _preferencesStore.Save(new UiPreferencesSnapshot
        {
            PreferredLibraryRoot = _session.HostOptions.LibraryRoot,
            ConverterMode = SelectedConverterMode,
            Fb2CngExecutablePath = IsBuiltInConverterSelected ? Fb2CngExecutablePath : null,
            Fb2CngConfigPath = IsBuiltInConverterSelected && !string.IsNullOrWhiteSpace(Fb2CngConfigPath) ? Fb2CngConfigPath : null,
            CustomConverterExecutablePath = IsCustomConverterSelected ? CustomConverterExecutablePath : null,
            CustomConverterArguments = IsCustomConverterSelected ? ParseCustomConverterArguments() : null,
            CustomConverterOutputMode = SelectedCustomConverterOutputMode
        });
        UiLogging.Information("Saved converter preferences for current library. LibraryRoot={LibraryRoot}", LibraryRoot);
        return Task.CompletedTask;
    }

    private bool CanSavePreferences() =>
        !HasConverterValidationError;

    private void UpdateConverterValidation()
    {
        ConverterValidationMessage = BuildConverterValidationMessage();
        RaisePropertyChanged(nameof(HasConverterValidationError));
        RaisePropertyChanged(nameof(ShowConverterHelperText));
        SavePreferencesCommand.RaiseCanExecuteChanged();
    }

    private string BuildConverterValidationMessage()
    {
        switch (SelectedConverterMode)
        {
            case UiConverterMode.Disabled:
                return string.Empty;

            case UiConverterMode.BuiltInFb2Cng:
                if (string.IsNullOrWhiteSpace(Fb2CngExecutablePath))
                {
                    return "Built-in fb2cng executable path is required.";
                }

                if (!Path.IsPathFullyQualified(Fb2CngExecutablePath))
                {
                    return "Built-in fb2cng executable path must be absolute.";
                }

                if (!string.IsNullOrWhiteSpace(Fb2CngConfigPath) && !Path.IsPathFullyQualified(Fb2CngConfigPath))
                {
                    return "Built-in fb2cng config path must be absolute.";
                }

                return string.Empty;

            case UiConverterMode.CustomCommand:
                if (string.IsNullOrWhiteSpace(CustomConverterExecutablePath))
                {
                    return "Custom converter executable path is required.";
                }

                if (!Path.IsPathFullyQualified(CustomConverterExecutablePath))
                {
                    return "Custom converter executable path must be absolute.";
                }

                if (ParseCustomConverterArguments().Length == 0)
                {
                    return "Custom converter arguments must contain at least one template entry.";
                }

                return string.Empty;

            default:
                return string.Empty;
        }
    }

    private string[] ParseCustomConverterArguments() =>
        CustomConverterArgumentsText
            .Split([Environment.NewLine, "\n"], StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);

    private void RaiseConverterPropertiesChanged()
    {
        RaisePropertyChanged(nameof(IsConverterDisabled));
        RaisePropertyChanged(nameof(IsBuiltInConverterSelected));
        RaisePropertyChanged(nameof(IsCustomConverterSelected));
        SavePreferencesCommand.RaiseCanExecuteChanged();
    }

    private async Task HandleImportCompletedSuccessfullyAsync()
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
}
