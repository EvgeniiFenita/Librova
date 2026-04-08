using Librova.UI.Desktop;
using Librova.UI.Logging;
using Librova.UI.Mvvm;
using Librova.UI.CoreHost;
using Librova.UI.Shell;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class FirstRunSetupViewModel : ObservableObject
{
    private readonly IPathSelectionService _pathSelectionService;
    private readonly IUiPreferencesStore _preferencesStore;
    private readonly Func<string, UiLibraryOpenMode, Task> _continueAsync;
    private readonly string _initialLibraryRoot;
    private readonly bool _requireDifferentLibraryRoot;
    private readonly bool _allowModeSelection;
    private readonly UiLibraryOpenMode? _lockedLibraryOpenMode;
    private string _libraryRoot;
    private string _statusText;
    private string _validationMessage = string.Empty;
    private bool _isBusy;
    private UiLibraryOpenMode _selectedLibraryOpenMode;

    public FirstRunSetupViewModel(
        string suggestedLibraryRoot,
        IPathSelectionService? pathSelectionService,
        IUiPreferencesStore? preferencesStore,
        Func<string, UiLibraryOpenMode, Task> continueAsync,
        bool requireDifferentLibraryRoot = false,
        bool allowModeSelection = false,
        UiLibraryOpenMode? lockedLibraryOpenMode = null)
    {
        _initialLibraryRoot = suggestedLibraryRoot;
        _requireDifferentLibraryRoot = requireDifferentLibraryRoot;
        _allowModeSelection = allowModeSelection;
        _lockedLibraryOpenMode = lockedLibraryOpenMode;
        _libraryRoot = suggestedLibraryRoot;
        _statusText = "Choose the managed library location for Librova.";
        _selectedLibraryOpenMode = lockedLibraryOpenMode ?? UiLibraryOpenMode.CreateNew;
        _pathSelectionService = pathSelectionService ?? new NullPathSelectionService();
        _preferencesStore = preferencesStore ?? UiPreferencesStore.CreateDefault();
        _continueAsync = continueAsync;

        BrowseLibraryRootCommand = new AsyncCommand(BrowseLibraryRootAsync, () => !IsBusy);
        SelectCreateLibraryModeCommand = new AsyncCommand(SelectCreateLibraryModeAsync, () => CanChangeLibraryOpenMode);
        SelectOpenLibraryModeCommand = new AsyncCommand(SelectOpenLibraryModeAsync, () => CanChangeLibraryOpenMode);
        ContinueCommand = new AsyncCommand(ContinueAsync, CanContinue, HandleContinueErrorAsync);
        UpdateValidation();
    }

    public AsyncCommand BrowseLibraryRootCommand { get; }
    public AsyncCommand SelectCreateLibraryModeCommand { get; }
    public AsyncCommand SelectOpenLibraryModeCommand { get; }
    public AsyncCommand ContinueCommand { get; }

    public string LibraryRoot
    {
        get => _libraryRoot;
        set
        {
            if (SetProperty(ref _libraryRoot, value))
            {
                UpdateValidation();
            }
        }
    }

    public string StatusText
    {
        get => _statusText;
        private set => SetProperty(ref _statusText, value);
    }

    public string ValidationMessage
    {
        get => _validationMessage;
        private set => SetProperty(ref _validationMessage, value);
    }

    public bool HasValidationError => !string.IsNullOrWhiteSpace(ValidationMessage);
    public bool ShowHelperText => !HasValidationError;
    public bool ShowLibraryOpenModeSelector => _allowModeSelection;
    public bool CanChangeLibraryOpenMode => !IsBusy && _allowModeSelection;
    public bool RequiresDifferentLibraryRoot => _requireDifferentLibraryRoot;
    public bool IsCreateLibraryModeSelected => EffectiveLibraryOpenMode is UiLibraryOpenMode.CreateNew;
    public bool IsOpenLibraryModeSelected => EffectiveLibraryOpenMode is UiLibraryOpenMode.OpenExisting;
    public string ContinueButtonText => EffectiveLibraryOpenMode is UiLibraryOpenMode.OpenExisting
        ? "Open Library"
        : "Continue";
    public string HelperText => _requireDifferentLibraryRoot
        ? "Choose a different library root. The current library could not be opened, so retrying the same path will reopen the same startup error."
        : EffectiveLibraryOpenMode is UiLibraryOpenMode.OpenExisting
            ? "Choose an absolute directory path to an existing Librova managed library. Librova will retry startup and only persist the library root after a successful shell launch."
            : "Choose an absolute directory path on an available drive. The folder may be new or empty; Librova will create its managed structure after a successful first launch.";

    public bool IsBusy
    {
        get => _isBusy;
        private set
        {
            if (SetProperty(ref _isBusy, value))
            {
                BrowseLibraryRootCommand.RaiseCanExecuteChanged();
                SelectCreateLibraryModeCommand.RaiseCanExecuteChanged();
                SelectOpenLibraryModeCommand.RaiseCanExecuteChanged();
                ContinueCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public async Task BrowseLibraryRootAsync()
    {
        var selectedPath = await _pathSelectionService.PickWorkingDirectoryAsync(CancellationToken.None);
        if (!string.IsNullOrWhiteSpace(selectedPath))
        {
            LibraryRoot = selectedPath;
        }
    }

    public Task SelectCreateLibraryModeAsync()
    {
        if (!CanChangeLibraryOpenMode)
        {
            return Task.CompletedTask;
        }

        _selectedLibraryOpenMode = UiLibraryOpenMode.CreateNew;
        UpdateValidation();
        return Task.CompletedTask;
    }

    public Task SelectOpenLibraryModeAsync()
    {
        if (!CanChangeLibraryOpenMode)
        {
            return Task.CompletedTask;
        }

        _selectedLibraryOpenMode = UiLibraryOpenMode.OpenExisting;
        UpdateValidation();
        return Task.CompletedTask;
    }

    private async Task ContinueAsync()
    {
        IsBusy = true;
        StatusText = "Saving setup and starting the native core host...";
        UiLogging.Information(
            "Continuing first-run setup with selected library root. LibraryRoot={LibraryRoot} LibraryOpenMode={LibraryOpenMode}",
            LibraryRoot,
            EffectiveLibraryOpenMode);

        try
        {
            await _continueAsync(LibraryRoot, EffectiveLibraryOpenMode);
            _preferencesStore.Save(UiPreferencesSnapshotBuilder.WithPreferredLibraryRoot(
                _preferencesStore.TryLoad(),
                LibraryRoot));
            UiLogging.Information("Saved first-run library root after successful shell startup. LibraryRoot={LibraryRoot}", LibraryRoot);
        }
        finally
        {
            IsBusy = false;
        }
    }

    private bool CanContinue() => !IsBusy && !HasValidationError && !string.IsNullOrWhiteSpace(LibraryRoot);

    private Task HandleContinueErrorAsync(Exception error)
    {
        StatusText = error.Message;
        return Task.CompletedTask;
    }

    private void UpdateValidation()
    {
        ValidationMessage = BuildValidationMessage();
        RaisePropertyChanged(nameof(HasValidationError));
        RaisePropertyChanged(nameof(ShowHelperText));
        RaisePropertyChanged(nameof(HelperText));
        RaisePropertyChanged(nameof(IsCreateLibraryModeSelected));
        RaisePropertyChanged(nameof(IsOpenLibraryModeSelected));
        RaisePropertyChanged(nameof(ContinueButtonText));
        ContinueCommand.RaiseCanExecuteChanged();
    }

    private string BuildValidationMessage()
    {
        var validationMessage = LibraryRootValidation.BuildValidationMessage(LibraryRoot);
        if (!string.IsNullOrWhiteSpace(validationMessage))
        {
            return validationMessage;
        }

        if (_requireDifferentLibraryRoot && AreEquivalentLibraryRoots(LibraryRoot, _initialLibraryRoot))
        {
            return "Choose a different library root. Retrying the same library will reopen the same startup error.";
        }

        return _lockedLibraryOpenMode is not null || _allowModeSelection
            ? LibraryRootInspection.BuildModeValidationMessage(LibraryRoot, EffectiveLibraryOpenMode)
            : string.Empty;
    }

    private UiLibraryOpenMode EffectiveLibraryOpenMode => _lockedLibraryOpenMode ?? _selectedLibraryOpenMode;

    private static bool AreEquivalentLibraryRoots(string left, string right)
    {
        if (string.IsNullOrWhiteSpace(left) || string.IsNullOrWhiteSpace(right))
        {
            return string.Equals(left, right, StringComparison.OrdinalIgnoreCase);
        }

        try
        {
            return string.Equals(
                Path.GetFullPath(left.Trim()),
                Path.GetFullPath(right.Trim()),
                StringComparison.OrdinalIgnoreCase);
        }
        catch (Exception)
        {
            return string.Equals(left.Trim(), right.Trim(), StringComparison.OrdinalIgnoreCase);
        }
    }
}
