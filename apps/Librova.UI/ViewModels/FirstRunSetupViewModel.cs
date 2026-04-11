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
    private readonly Func<string, UiLibraryOpenMode, Task<string>> _modeValidationAsync;
    private readonly string _initialLibraryRoot;
    private readonly bool _requireDifferentLibraryRoot;
    private readonly bool _allowModeSelection;
    private readonly UiLibraryOpenMode? _lockedLibraryOpenMode;
    private string _libraryRoot;
    private string _statusText;
    private string _validationMessage = string.Empty;
    private bool _isBusy;
    private UiLibraryOpenMode _selectedLibraryOpenMode;
    private int _modeValidationVersion;

    public FirstRunSetupViewModel(
        string suggestedLibraryRoot,
        IPathSelectionService? pathSelectionService,
        IUiPreferencesStore? preferencesStore,
        Func<string, UiLibraryOpenMode, Task> continueAsync,
        bool requireDifferentLibraryRoot = false,
        bool allowModeSelection = false,
        UiLibraryOpenMode? lockedLibraryOpenMode = null,
        Func<string, UiLibraryOpenMode, Task<string>>? modeValidationAsync = null)
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
        _modeValidationAsync = modeValidationAsync ?? ((libraryRoot, mode) =>
            Task.Run(() => LibraryRootInspection.BuildModeValidationMessage(libraryRoot, mode)));

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
        if (string.IsNullOrWhiteSpace(selectedPath))
        {
            return;
        }

        // Yield one dispatcher cycle so Avalonia can process the EnableWindow(TRUE)
        // message that was posted when the folder picker dialog closed. Without this
        // yield, our synchronous continuation code runs before the UI thread pump
        // handles the re-enable, leaving the parent window in Win32-modal-disabled
        // state long enough to produce the system "blocked window" beep on click.
        await Task.Yield();

        _libraryRoot = selectedPath;
        RaisePropertyChanged(nameof(LibraryRoot));

        // Apply cheap synchronous checks immediately — this clears any validation
        // error that was showing for the previous path and re-enables Continue
        // without waiting for the filesystem check. Pre-refactor, UpdateValidation()
        // did all of this synchronously; now the fast portion runs here and the
        // heavy portion follows separately on a thread pool so the browse command
        // itself can finish immediately after the picker closes.
        var quickMsg = BuildCheapValidationMessage(selectedPath);
        ApplyValidationState(quickMsg);

        if (quickMsg.Length == 0 && (_lockedLibraryOpenMode is not null || _allowModeSelection))
        {
            ScheduleModeValidation(selectedPath, EffectiveLibraryOpenMode);
        }

        UiLogging.Information(
            "BrowseLibraryRoot applied. LibraryRoot={LibraryRoot} Validation={Validation}",
            selectedPath,
            ValidationMessage.Length == 0 ? "<ok>" : ValidationMessage);
    }

    public Task SelectCreateLibraryModeAsync()
    {
        if (!CanChangeLibraryOpenMode)
        {
            return Task.CompletedTask;
        }

        _selectedLibraryOpenMode = UiLibraryOpenMode.CreateNew;
        ApplyModeSelectionState();
        var quickMsg = BuildCheapValidationMessage(_libraryRoot);
        ApplyValidationState(quickMsg);

        if (quickMsg.Length == 0)
        {
            ScheduleModeValidation(_libraryRoot, EffectiveLibraryOpenMode);
        }

        return Task.CompletedTask;
    }

    public Task SelectOpenLibraryModeAsync()
    {
        if (!CanChangeLibraryOpenMode)
        {
            return Task.CompletedTask;
        }

        _selectedLibraryOpenMode = UiLibraryOpenMode.OpenExisting;
        ApplyModeSelectionState();
        var quickMsg = BuildCheapValidationMessage(_libraryRoot);
        ApplyValidationState(quickMsg);

        if (quickMsg.Length == 0)
        {
            ScheduleModeValidation(_libraryRoot, EffectiveLibraryOpenMode);
        }

        return Task.CompletedTask;
    }

    private async Task ContinueAsync()
    {
        var validationMessage = await ValidateSelectedLibraryRootForContinueAsync();
        if (!string.IsNullOrWhiteSpace(validationMessage))
        {
            ApplyValidationState(validationMessage);
            StatusText = validationMessage;
            UiLogging.Warning(
                "Rejected first-run continue because the selected library root failed validation. LibraryRoot={LibraryRoot} Validation={Validation}",
                LibraryRoot,
                validationMessage);
            return;
        }

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

    private void UpdateValidation() => ApplyValidationState(BuildValidationMessage());

    private void ApplyValidationState(string validationMessage)
    {
        ValidationMessage = validationMessage;
        RaisePropertyChanged(nameof(HasValidationError));
        RaisePropertyChanged(nameof(ShowHelperText));
        RaisePropertyChanged(nameof(HelperText));
        RaisePropertyChanged(nameof(IsCreateLibraryModeSelected));
        RaisePropertyChanged(nameof(IsOpenLibraryModeSelected));
        RaisePropertyChanged(nameof(ContinueButtonText));
        ContinueCommand.RaiseCanExecuteChanged();
    }

    private void ApplyModeSelectionState()
    {
        RaisePropertyChanged(nameof(IsCreateLibraryModeSelected));
        RaisePropertyChanged(nameof(IsOpenLibraryModeSelected));
        RaisePropertyChanged(nameof(ContinueButtonText));
    }

    // Synchronous path: used by property setter and constructor. Includes filesystem
    // I/O (directory enumeration); must not be called from an async Browse/mode-switch
    // path where the UI thread needs to remain responsive.
    private string BuildValidationMessage()
    {
        var msg = BuildCheapValidationMessage(LibraryRoot);
        if (msg.Length > 0)
            return msg;

        return _lockedLibraryOpenMode is not null || _allowModeSelection
            ? LibraryRootInspection.BuildModeValidationMessage(LibraryRoot, EffectiveLibraryOpenMode)
            : string.Empty;
    }

    // Format and equality checks only — no filesystem I/O. Called synchronously
    // before any async filesystem check so the UI updates without delay.
    private string BuildCheapValidationMessage(string libraryRoot)
    {
        var msg = LibraryRootValidation.BuildValidationMessage(libraryRoot);
        if (!string.IsNullOrWhiteSpace(msg))
            return msg;

        if (_requireDifferentLibraryRoot && AreEquivalentLibraryRoots(libraryRoot, _initialLibraryRoot))
            return "Choose a different library root. Retrying the same library will reopen the same startup error.";

        return string.Empty;
    }

    private UiLibraryOpenMode EffectiveLibraryOpenMode => _lockedLibraryOpenMode ?? _selectedLibraryOpenMode;

    private void ScheduleModeValidation(string libraryRoot, UiLibraryOpenMode mode)
    {
        var validationVersion = Interlocked.Increment(ref _modeValidationVersion);
        _ = RunModeValidationAsync(libraryRoot, mode, validationVersion);
    }

    private async Task RunModeValidationAsync(string libraryRoot, UiLibraryOpenMode mode, int validationVersion)
    {
        string validationMessage;
        try
        {
            validationMessage = await _modeValidationAsync(libraryRoot, mode);
        }
        catch (Exception error)
        {
            UiLogging.Warning(
                error,
                "Background first-run library-mode validation failed. LibraryRoot={LibraryRoot} LibraryOpenMode={LibraryOpenMode}",
                libraryRoot,
                mode);
            return;
        }

        if (validationVersion != Volatile.Read(ref _modeValidationVersion)
            || !string.Equals(_libraryRoot, libraryRoot, StringComparison.Ordinal)
            || EffectiveLibraryOpenMode != mode)
        {
            UiLogging.Information(
                "Discarded stale first-run library-mode validation result. LibraryRoot={LibraryRoot} LibraryOpenMode={LibraryOpenMode}",
                libraryRoot,
                mode);
            return;
        }

        ApplyValidationState(validationMessage);
        UiLogging.Information(
            "Completed background first-run library-mode validation. LibraryRoot={LibraryRoot} LibraryOpenMode={LibraryOpenMode} Validation={Validation}",
            libraryRoot,
            mode,
            validationMessage.Length == 0 ? "<ok>" : validationMessage);
    }

    private async Task<string> ValidateSelectedLibraryRootForContinueAsync()
    {
        var quickValidationMessage = BuildCheapValidationMessage(LibraryRoot);
        if (!string.IsNullOrWhiteSpace(quickValidationMessage))
        {
            return quickValidationMessage;
        }

        if (_lockedLibraryOpenMode is null && !_allowModeSelection)
        {
            return string.Empty;
        }

        var libraryRoot = LibraryRoot;
        var mode = EffectiveLibraryOpenMode;
        return await _modeValidationAsync(libraryRoot, mode);
    }

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
        catch (Exception error) when (error is ArgumentException or NotSupportedException or PathTooLongException)
        {
            UiLogging.Warning(
                error,
                "Falling back to trimmed path comparison after full-path normalization failed. Left={LeftPath} Right={RightPath}",
                left,
                right);
            return string.Equals(left.Trim(), right.Trim(), StringComparison.OrdinalIgnoreCase);
        }
    }
}
