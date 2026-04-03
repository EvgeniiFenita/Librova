using Librova.UI.Desktop;
using Librova.UI.Logging;
using Librova.UI.Mvvm;
using Librova.UI.Shell;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class FirstRunSetupViewModel : ObservableObject
{
    private readonly IPathSelectionService _pathSelectionService;
    private readonly IUiPreferencesStore _preferencesStore;
    private readonly Func<string, Task> _continueAsync;
    private readonly Func<string, string>? _additionalValidation;
    private readonly string _initialLibraryRoot;
    private readonly bool _requireDifferentLibraryRoot;
    private string _libraryRoot;
    private string _statusText;
    private string _validationMessage = string.Empty;
    private bool _isBusy;

    public FirstRunSetupViewModel(
        string suggestedLibraryRoot,
        IPathSelectionService? pathSelectionService,
        IUiPreferencesStore? preferencesStore,
        Func<string, Task> continueAsync,
        bool requireDifferentLibraryRoot = false,
        Func<string, string>? additionalValidation = null)
    {
        _initialLibraryRoot = suggestedLibraryRoot;
        _requireDifferentLibraryRoot = requireDifferentLibraryRoot;
        _libraryRoot = suggestedLibraryRoot;
        _statusText = "Choose the managed library location for Librova.";
        _pathSelectionService = pathSelectionService ?? new NullPathSelectionService();
        _preferencesStore = preferencesStore ?? UiPreferencesStore.CreateDefault();
        _continueAsync = continueAsync;
        _additionalValidation = additionalValidation;

        BrowseLibraryRootCommand = new AsyncCommand(BrowseLibraryRootAsync, () => !IsBusy);
        ContinueCommand = new AsyncCommand(ContinueAsync, CanContinue, HandleContinueErrorAsync);
        UpdateValidation();
    }

    public AsyncCommand BrowseLibraryRootCommand { get; }
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
    public bool RequiresDifferentLibraryRoot => _requireDifferentLibraryRoot;
    public string HelperText => _requireDifferentLibraryRoot
        ? "Choose a different library root. The current library could not be opened, so retrying the same path will reopen the same startup error."
        : "Choose an absolute directory path on an available drive. Librova will retry startup and only persist the library root after a successful shell launch.";

    public bool IsBusy
    {
        get => _isBusy;
        private set
        {
            if (SetProperty(ref _isBusy, value))
            {
                BrowseLibraryRootCommand.RaiseCanExecuteChanged();
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

    private async Task ContinueAsync()
    {
        IsBusy = true;
        StatusText = "Saving setup and starting the native core host...";
        UiLogging.Information("Continuing first-run setup with selected library root. LibraryRoot={LibraryRoot}", LibraryRoot);

        try
        {
            await _continueAsync(LibraryRoot);
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

        if (_additionalValidation is not null)
        {
            return _additionalValidation(LibraryRoot);
        }

        return string.Empty;
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
        catch (Exception)
        {
            return string.Equals(left.Trim(), right.Trim(), StringComparison.OrdinalIgnoreCase);
        }
    }
}
