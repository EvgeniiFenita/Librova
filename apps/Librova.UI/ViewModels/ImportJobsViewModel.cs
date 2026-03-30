using Librova.UI.Desktop;
using Librova.UI.ImportJobs;
using Librova.UI.Logging;
using Librova.UI.Mvvm;
using System;
using System.ComponentModel;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class ImportJobsViewModel : ObservableObject
{
    private readonly IImportJobsService _importJobsService;
    private readonly IPathSelectionService _pathSelectionService;
    private string _sourcePath = string.Empty;
    private string _workingDirectory = string.Empty;
    private bool _allowProbableDuplicates;
    private string _statusText = "Idle";
    private string _sourceValidationMessage = string.Empty;
    private string _workingDirectoryValidationMessage = string.Empty;
    private bool _isBusy;
    private ulong? _lastJobId;
    private ImportJobResultModel? _lastResult;
    private string _resultSummaryText = "No completed job yet.";
    private string _warningsText = "No warnings.";
    private string _errorText = "No error.";
    private CancellationTokenSource? _activeImportCancellation;
    public event Func<Task>? ImportCompletedSuccessfully;
    public event Action<bool>? ImportActivityChanged;

    public ImportJobsViewModel(IImportJobsService importJobsService, IPathSelectionService? pathSelectionService = null)
    {
        _importJobsService = importJobsService;
        _pathSelectionService = pathSelectionService ?? new NullPathSelectionService();
        PropertyChanged += OnPropertyChanged;
        StartImportCommand = new AsyncCommand(StartImportAsync, CanStartImport, HandleCommandErrorAsync);
        RefreshCommand = new AsyncCommand(RefreshCurrentAsync, CanRefresh, HandleCommandErrorAsync);
        CancelImportCommand = new AsyncCommand(CancelCurrentAsync, CanCancel, HandleCommandErrorAsync);
        RemoveJobCommand = new AsyncCommand(RemoveCurrentAsync, CanRemove, HandleCommandErrorAsync);
        SelectSourceAndImportCommand = new AsyncCommand(SelectSourceAndImportAsync, () => !IsBusy, HandleCommandErrorAsync);
        BrowseSourceCommand = new AsyncCommand(BrowseSourceAsync, () => !IsBusy, HandleCommandErrorAsync);
        BrowseWorkingDirectoryCommand = new AsyncCommand(BrowseWorkingDirectoryAsync, () => !IsBusy, HandleCommandErrorAsync);
        UpdateValidationState();
    }

    public string SourcePath
    {
        get => _sourcePath;
        set
        {
            if (SetProperty(ref _sourcePath, value))
            {
                UpdateValidationState();
                StartImportCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public string WorkingDirectory
    {
        get => _workingDirectory;
        set
        {
            if (SetProperty(ref _workingDirectory, value))
            {
                UpdateValidationState();
                StartImportCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public string StatusText
    {
        get => _statusText;
        private set => SetProperty(ref _statusText, value);
    }

    public string SourceValidationMessage
    {
        get => _sourceValidationMessage;
        private set => SetProperty(ref _sourceValidationMessage, value);
    }

    public string WorkingDirectoryValidationMessage
    {
        get => _workingDirectoryValidationMessage;
        private set => SetProperty(ref _workingDirectoryValidationMessage, value);
    }

    public bool HasSourceValidationError => !string.IsNullOrWhiteSpace(SourceValidationMessage);
    public bool HasWorkingDirectoryValidationError => !string.IsNullOrWhiteSpace(WorkingDirectoryValidationMessage);
    public bool ShowSourceHelperText => !HasSourceValidationError;
    public bool ShowWorkingDirectoryHelperText => !HasWorkingDirectoryValidationError;
    public bool HasSelectedSource => !string.IsNullOrWhiteSpace(SourcePath);
    public bool ShowIdleImportPicker => !IsBusy;
    public bool ShowRunningImportState => IsBusy;
    public bool HasImportResult => LastResult is not null;
    public string SelectedSourceLabel => string.IsNullOrWhiteSpace(SourcePath)
        ? "No file selected yet."
        : Path.GetFileName(SourcePath);
    public string SelectedSourcePathText => string.IsNullOrWhiteSpace(SourcePath)
        ? "Drop a local .fb2, .epub, or .zip file here or use Select Files..."
        : SourcePath;

    public bool IsBusy
    {
        get => _isBusy;
        private set
        {
            if (SetProperty(ref _isBusy, value))
            {
                StartImportCommand.RaiseCanExecuteChanged();
                RefreshCommand.RaiseCanExecuteChanged();
                CancelImportCommand.RaiseCanExecuteChanged();
                RemoveJobCommand.RaiseCanExecuteChanged();
                SelectSourceAndImportCommand.RaiseCanExecuteChanged();
                BrowseSourceCommand.RaiseCanExecuteChanged();
                BrowseWorkingDirectoryCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public bool AllowProbableDuplicates
    {
        get => _allowProbableDuplicates;
        set => SetProperty(ref _allowProbableDuplicates, value);
    }

    public ulong? LastJobId
    {
        get => _lastJobId;
        private set
        {
            if (SetProperty(ref _lastJobId, value))
            {
                RefreshCommand.RaiseCanExecuteChanged();
                CancelImportCommand.RaiseCanExecuteChanged();
                RemoveJobCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public ImportJobResultModel? LastResult
    {
        get => _lastResult;
        private set
        {
            if (SetProperty(ref _lastResult, value))
            {
                UpdateDerivedResultState();
            }
        }
    }

    public string ResultSummaryText
    {
        get => _resultSummaryText;
        private set => SetProperty(ref _resultSummaryText, value);
    }

    public string WarningsText
    {
        get => _warningsText;
        private set => SetProperty(ref _warningsText, value);
    }

    public string ErrorText
    {
        get => _errorText;
        private set => SetProperty(ref _errorText, value);
    }

    public AsyncCommand StartImportCommand { get; }
    public AsyncCommand RefreshCommand { get; }
    public AsyncCommand CancelImportCommand { get; }
    public AsyncCommand RemoveJobCommand { get; }
    public AsyncCommand SelectSourceAndImportCommand { get; }
    public AsyncCommand BrowseSourceCommand { get; }
    public AsyncCommand BrowseWorkingDirectoryCommand { get; }

    public Shell.ShellStateSnapshot CreateStateSnapshot() =>
        new()
        {
            SourcePath = SourcePath,
            WorkingDirectory = WorkingDirectory,
            AllowProbableDuplicates = AllowProbableDuplicates
        };

    public void ApplyDroppedSourcePath(string? sourcePath)
    {
        if (string.IsNullOrWhiteSpace(sourcePath))
        {
            UiLogging.Warning("Ignored dropped source path because it was empty.");
            return;
        }

        SourcePath = sourcePath;
        UiLogging.Information("Applied dropped source path to import shell.");
    }

    public async Task ApplyDroppedSourcePathAndStartAsync(string? sourcePath)
    {
        ApplyDroppedSourcePath(sourcePath);

        if (CanStartImport())
        {
            await StartImportAsync();
        }
    }

    public async Task StartImportAsync()
    {
        IsBusy = true;
        StatusText = "Starting import...";
        LastResult = null;
        _activeImportCancellation?.Dispose();
        _activeImportCancellation = new CancellationTokenSource();

        try
        {
            var jobId = await _importJobsService.StartAsync(
                new StartImportRequestModel
                {
                    SourcePath = SourcePath,
                    WorkingDirectory = WorkingDirectory,
                    AllowProbableDuplicates = AllowProbableDuplicates
                },
                TimeSpan.FromSeconds(5),
                _activeImportCancellation.Token);

            LastJobId = jobId;
            StatusText = $"Import job {jobId} started.";
            await PollUntilCompletedAsync(jobId, _activeImportCancellation.Token);
        }
        finally
        {
            _activeImportCancellation?.Dispose();
            _activeImportCancellation = null;
            IsBusy = false;
        }
    }

    public async Task RefreshAsync(ulong? jobId = null)
    {
        var effectiveJobId = jobId ?? LastJobId;
        if (effectiveJobId is null)
        {
            return;
        }

        using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(10));
        var result = await _importJobsService.TryGetResultAsync(
            effectiveJobId.Value,
            TimeSpan.FromSeconds(5),
            cancellation.Token);

        if (result is not null)
        {
            LastResult = result;
            StatusText = result.Snapshot.Message.Length == 0
                ? result.Snapshot.Status.ToString()
                : result.Snapshot.Message;
            return;
        }

        var snapshot = await _importJobsService.TryGetSnapshotAsync(
            effectiveJobId.Value,
            TimeSpan.FromSeconds(5),
            cancellation.Token);

        if (snapshot is not null)
        {
            StatusText = snapshot.Message.Length == 0
                ? snapshot.Status.ToString()
                : snapshot.Message;
        }
    }

    public Task RefreshCurrentAsync() => RefreshAsync();

    public async Task CancelCurrentAsync()
    {
        if (LastJobId is null)
        {
            return;
        }

        var cancellation = _activeImportCancellation ?? new CancellationTokenSource(TimeSpan.FromSeconds(10));
        var accepted = await _importJobsService.CancelAsync(
            LastJobId.Value,
            TimeSpan.FromSeconds(5),
            cancellation.Token);

        if (!ReferenceEquals(cancellation, _activeImportCancellation))
        {
            cancellation.Dispose();
        }

        if (accepted)
        {
            StatusText = $"Import job {LastJobId.Value} cancellation requested.";
            return;
        }

        StatusText = $"Import job {LastJobId.Value} could not be cancelled.";
    }

    public async Task RemoveCurrentAsync()
    {
        if (LastJobId is null)
        {
            return;
        }

        using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(10));
        var removed = await _importJobsService.RemoveAsync(
            LastJobId.Value,
            TimeSpan.FromSeconds(5),
            cancellation.Token);

        if (!removed)
        {
            StatusText = $"Import job {LastJobId.Value} could not be removed.";
            return;
        }

        var removedJobId = LastJobId.Value;
        LastJobId = null;
        LastResult = null;
        StatusText = $"Import job {removedJobId} was removed.";
    }

    public async Task BrowseSourceAsync()
    {
        using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(30));
        var selectedPath = await _pathSelectionService.PickSourceFileAsync(cancellation.Token);
        if (!string.IsNullOrWhiteSpace(selectedPath))
        {
            SourcePath = selectedPath;
        }
    }

    public async Task SelectSourceAndImportAsync()
    {
        using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(30));
        var selectedPath = await _pathSelectionService.PickSourceFileAsync(cancellation.Token);
        if (string.IsNullOrWhiteSpace(selectedPath))
        {
            return;
        }

        SourcePath = selectedPath;
        if (CanStartImport())
        {
            await StartImportAsync();
        }
    }

    public async Task BrowseWorkingDirectoryAsync()
    {
        using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(30));
        var selectedPath = await _pathSelectionService.PickWorkingDirectoryAsync(cancellation.Token);
        if (!string.IsNullOrWhiteSpace(selectedPath))
        {
            WorkingDirectory = selectedPath;
        }
    }

    private async Task PollUntilCompletedAsync(ulong jobId, CancellationToken cancellationToken)
    {
        while (true)
        {
            cancellationToken.ThrowIfCancellationRequested();

            var result = await _importJobsService.TryGetResultAsync(
                jobId,
                TimeSpan.FromSeconds(5),
                cancellationToken);

            if (result is not null)
            {
                LastResult = result;
                StatusText = result.Snapshot.Message.Length == 0
                    ? result.Snapshot.Status.ToString()
                    : result.Snapshot.Message;
                await RaiseImportCompletedSuccessfullyAsync(result);
                return;
            }

            var snapshot = await _importJobsService.TryGetSnapshotAsync(
                jobId,
                TimeSpan.FromSeconds(5),
                cancellationToken);

            if (snapshot is not null)
            {
                StatusText = snapshot.Message.Length == 0
                    ? snapshot.Status.ToString()
                    : snapshot.Message;
            }

            await Task.Delay(200, cancellationToken);
        }
    }

    private Task HandleCommandErrorAsync(Exception error)
    {
        StatusText = error is OperationCanceledException
            ? "Import was cancelled."
            : error.Message.Length == 0
                ? "Import failed."
                : error.Message;
        return Task.CompletedTask;
    }

    private void UpdateDerivedResultState()
    {
        if (LastResult is null)
        {
            ResultSummaryText = "No completed job yet.";
            WarningsText = "No warnings.";
            ErrorText = "No error.";
            return;
        }

        var summary = LastResult.Summary;
        ResultSummaryText = summary is null
            ? "No import summary available."
            : $"Imported {summary.ImportedEntries} of {summary.TotalEntries}; failed {summary.FailedEntries}; skipped {summary.SkippedEntries}.";

        var warnings = summary?.Warnings ?? LastResult.Snapshot.Warnings;
        WarningsText = warnings.Count == 0
            ? "No warnings."
            : string.Join(Environment.NewLine, warnings);

        ErrorText = LastResult.Error is null
            ? "No error."
            : $"{LastResult.Error.Code}: {LastResult.Error.Message}";
    }

    private void OnPropertyChanged(object? sender, PropertyChangedEventArgs eventArgs)
    {
        if (eventArgs.PropertyName is nameof(SourcePath))
        {
            RaisePropertyChanged(nameof(HasSelectedSource));
            RaisePropertyChanged(nameof(SelectedSourceLabel));
            RaisePropertyChanged(nameof(SelectedSourcePathText));
        }
        else if (eventArgs.PropertyName is nameof(IsBusy))
        {
            RaisePropertyChanged(nameof(ShowIdleImportPicker));
            RaisePropertyChanged(nameof(ShowRunningImportState));
            ImportActivityChanged?.Invoke(IsBusy);
        }
        else if (eventArgs.PropertyName is nameof(LastResult))
        {
            RaisePropertyChanged(nameof(HasImportResult));
        }
    }

    private async Task RaiseImportCompletedSuccessfullyAsync(ImportJobResultModel result)
    {
        if (ImportCompletedSuccessfully is null)
        {
            return;
        }

        if (result.Snapshot.Status is not ImportJobStatusModel.Completed)
        {
            return;
        }

        if (result.Summary is null || result.Summary.ImportedEntries == 0)
        {
            return;
        }

        await ImportCompletedSuccessfully.Invoke();
    }

    private bool CanStartImport() =>
        !IsBusy
        && !string.IsNullOrWhiteSpace(SourcePath)
        && !string.IsNullOrWhiteSpace(WorkingDirectory)
        && !HasSourceValidationError
        && !HasWorkingDirectoryValidationError;

    private bool CanRefresh() => LastJobId.HasValue && !IsBusy;

    private bool CanCancel() => LastJobId.HasValue && IsBusy;

    private bool CanRemove() => LastJobId.HasValue && !IsBusy;

    private void UpdateValidationState()
    {
        SourceValidationMessage = BuildSourceValidationMessage(SourcePath);
        WorkingDirectoryValidationMessage = BuildWorkingDirectoryValidationMessage(WorkingDirectory);
        RaisePropertyChanged(nameof(HasSourceValidationError));
        RaisePropertyChanged(nameof(HasWorkingDirectoryValidationError));
        RaisePropertyChanged(nameof(ShowSourceHelperText));
        RaisePropertyChanged(nameof(ShowWorkingDirectoryHelperText));
    }

    private static string BuildSourceValidationMessage(string sourcePath)
    {
        if (string.IsNullOrWhiteSpace(sourcePath))
        {
            return string.Empty;
        }

        if (!Path.IsPathFullyQualified(sourcePath))
        {
            return "Use an absolute source file path.";
        }

        if (Directory.Exists(sourcePath))
        {
            return "Source path must point to a file, not a directory.";
        }

        if (!File.Exists(sourcePath))
        {
            return "Source file does not exist.";
        }

        var extension = Path.GetExtension(sourcePath);
        if (!extension.Equals(".fb2", StringComparison.OrdinalIgnoreCase)
            && !extension.Equals(".epub", StringComparison.OrdinalIgnoreCase)
            && !extension.Equals(".zip", StringComparison.OrdinalIgnoreCase))
        {
            return "Supported source types are .fb2, .epub, and .zip.";
        }

        return string.Empty;
    }

    private static string BuildWorkingDirectoryValidationMessage(string workingDirectory)
    {
        if (string.IsNullOrWhiteSpace(workingDirectory))
        {
            return string.Empty;
        }

        if (!Path.IsPathFullyQualified(workingDirectory))
        {
            return "Use an absolute working directory path.";
        }

        if (File.Exists(workingDirectory))
        {
            return "Working directory must not point to a file.";
        }

        return string.Empty;
    }
}
