using Librova.UI.Desktop;
using Librova.UI.ImportJobs;
using Librova.UI.Logging;
using Librova.UI.Mvvm;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class ImportJobsViewModel : ObservableObject
{
    private readonly IImportJobsService _importJobsService;
    private readonly IPathSelectionService _pathSelectionService;
    private IReadOnlyList<string> _sourcePaths = [];
    private string _workingDirectory = string.Empty;
    private bool _allowProbableDuplicates;
    private bool _forceEpubConversionOnImport;
    private bool _hasConfiguredConverter;
    private string _statusText = "Idle";
    private string _sourceValidationMessage = string.Empty;
    private string _workingDirectoryValidationMessage = string.Empty;
    private bool _isBusy;
    private ulong? _lastJobId;
    private ImportJobResultModel? _lastResult;
    private string _progressSummaryText = "Progress details will appear here.";
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
        SelectDirectoryAndImportCommand = new AsyncCommand(SelectDirectoryAndImportAsync, () => !IsBusy, HandleCommandErrorAsync);
        BrowseSourceCommand = new AsyncCommand(BrowseSourceAsync, () => !IsBusy, HandleCommandErrorAsync);
        BrowseWorkingDirectoryCommand = new AsyncCommand(BrowseWorkingDirectoryAsync, () => !IsBusy, HandleCommandErrorAsync);
        UpdateValidationState();
    }

    public string SourcePath
    {
        get => SourcePaths.FirstOrDefault() ?? string.Empty;
        set => SetSourcePaths(string.IsNullOrWhiteSpace(value) ? [] : [value]);
    }

    public IReadOnlyList<string> SourcePaths
    {
        get => _sourcePaths;
        private set
        {
            _sourcePaths = value;
            UpdateValidationState();
            StartImportCommand.RaiseCanExecuteChanged();
            RaisePropertyChanged();
            RaisePropertyChanged(nameof(SourcePath));
            RaisePropertyChanged(nameof(HasSelectedSource));
            RaisePropertyChanged(nameof(SelectedSourceLabel));
            RaisePropertyChanged(nameof(SelectedSourcePathText));
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
    public bool HasSelectedSource => SourcePaths.Count > 0;
    public bool ShowIdleImportPicker => !IsBusy;
    public bool ShowRunningImportState => IsBusy;
    public bool CanAcceptNewSources => !IsBusy;
    public bool HasImportResult => LastResult is not null;
    public string SelectedSourceLabel => SourcePaths.Count switch
    {
        0 => "No source selected yet.",
        1 => Path.GetFileName(SourcePath.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)),
        _ => $"{SourcePaths.Count} selected sources"
    };
    public string SelectedSourcePathText => SourcePaths.Count switch
    {
        0 => "Drop local .fb2, .epub, or .zip files or folders here, or use Select Files... / Select Folder...",
        1 => SourcePaths[0],
        _ => string.Join(Environment.NewLine, SourcePaths.Take(5))
             + (SourcePaths.Count > 5 ? Environment.NewLine + $"...and {SourcePaths.Count - 5} more." : string.Empty)
    };

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
                SelectDirectoryAndImportCommand.RaiseCanExecuteChanged();
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

    public bool ForceEpubConversionOnImport
    {
        get => _forceEpubConversionOnImport;
        set => SetProperty(ref _forceEpubConversionOnImport, HasConfiguredConverter && value);
    }

    public bool HasConfiguredConverter
    {
        get => _hasConfiguredConverter;
        set
        {
            if (SetProperty(ref _hasConfiguredConverter, value))
            {
                RaisePropertyChanged(nameof(ShowForceEpubConversionOption));
                if (!value)
                {
                    ForceEpubConversionOnImport = false;
                }
            }
        }
    }

    public bool ShowForceEpubConversionOption => HasConfiguredConverter;

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

    public string ProgressSummaryText
    {
        get => _progressSummaryText;
        private set => SetProperty(ref _progressSummaryText, value);
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
    public AsyncCommand SelectDirectoryAndImportCommand { get; }
    public AsyncCommand BrowseSourceCommand { get; }
    public AsyncCommand BrowseWorkingDirectoryCommand { get; }

    public Shell.ShellStateSnapshot CreateStateSnapshot() =>
        new()
        {
            SourcePaths = SourcePaths.ToArray(),
            WorkingDirectory = WorkingDirectory,
            AllowProbableDuplicates = AllowProbableDuplicates
        };

    public void ApplyDroppedSourcePath(string? sourcePath)
        => ApplyDroppedSourcePaths(string.IsNullOrWhiteSpace(sourcePath) ? [] : [sourcePath]);

    public void ApplyDroppedSourcePaths(IReadOnlyList<string> sourcePaths)
    {
        if (IsBusy)
        {
            UiLogging.Warning("Ignored dropped source paths because an import job is already running.");
            return;
        }

        if (sourcePaths.Count == 0 || sourcePaths.All(string.IsNullOrWhiteSpace))
        {
            UiLogging.Warning("Ignored dropped source paths because they were empty.");
            return;
        }

        SetSourcePaths(sourcePaths);
        UiLogging.Information("Applied {SourceCount} dropped source path(s) to import shell.", SourcePaths.Count);
    }

    public Task ApplyDroppedSourcePathAndStartAsync(string? sourcePath)
        => ApplyDroppedSourcePathsAndStartAsync(string.IsNullOrWhiteSpace(sourcePath) ? [] : [sourcePath]);

    public async Task ApplyDroppedSourcePathsAndStartAsync(IReadOnlyList<string> sourcePaths)
    {
        ApplyDroppedSourcePaths(sourcePaths);

        if (CanStartImport())
        {
            try
            {
                await StartImportAsync();
            }
            catch (Exception error)
            {
                await HandleCommandErrorAsync(error);
            }
        }
    }

    public async Task StartImportAsync()
    {
        IsBusy = true;
        StatusText = "Starting import...";
        ProgressSummaryText = "Preparing import workload...";
        LastResult = null;
        _activeImportCancellation?.Dispose();
        _activeImportCancellation = new CancellationTokenSource();

        try
        {
            var jobId = await _importJobsService.StartAsync(
                new StartImportRequestModel
                {
                    SourcePaths = SourcePaths.ToArray(),
                    WorkingDirectory = WorkingDirectory,
                    AllowProbableDuplicates = AllowProbableDuplicates,
                    ForceEpubConversionOnImport = ForceEpubConversionOnImport
                },
                TimeSpan.FromSeconds(5),
                _activeImportCancellation.Token);

            LastJobId = jobId;
            StatusText = $"Import job {jobId} started.";
            UiLogging.Information("Import job {JobId} started from UI shell.", jobId);
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
            ApplyTerminalResult(result);
            return;
        }

        var snapshot = await _importJobsService.TryGetSnapshotAsync(
            effectiveJobId.Value,
            TimeSpan.FromSeconds(5),
            cancellation.Token);

        if (snapshot is not null)
        {
            ApplySnapshot(snapshot);
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

        StatusText = accepted
            ? $"Import job {LastJobId.Value} cancellation requested."
            : $"Import job {LastJobId.Value} could not be cancelled.";
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
        var selectedPaths = await _pathSelectionService.PickSourceFilesAsync(cancellation.Token);
        if (selectedPaths.Count > 0)
        {
            SetSourcePaths(selectedPaths);
        }
    }

    public async Task SelectSourceAndImportAsync()
    {
        using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(30));
        var selectedPaths = await _pathSelectionService.PickSourceFilesAsync(cancellation.Token);
        if (selectedPaths.Count == 0)
        {
            return;
        }

        SetSourcePaths(selectedPaths);
        if (CanStartImport())
        {
            await StartImportAsync();
        }
    }

    public async Task SelectDirectoryAndImportAsync()
    {
        using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(30));
        var selectedPath = await _pathSelectionService.PickSourceDirectoryAsync(cancellation.Token);
        if (string.IsNullOrWhiteSpace(selectedPath))
        {
            return;
        }

        SetSourcePaths([selectedPath]);
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
                ApplyTerminalResult(result);
                LogTerminalResult(jobId, result);
                await RaiseImportCompletedSuccessfullyAsync(result);
                return;
            }

            var snapshot = await _importJobsService.TryGetSnapshotAsync(
                jobId,
                TimeSpan.FromSeconds(5),
                cancellationToken);

            if (snapshot is not null)
            {
                ApplySnapshot(snapshot);
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
        UiLogging.Error(error, "Import workflow failed in UI shell.");
        return Task.CompletedTask;
    }

    private static void LogTerminalResult(ulong jobId, ImportJobResultModel result)
    {
        if (result.Snapshot.Status is ImportJobStatusModel.Completed)
        {
            UiLogging.Information(
                "Import job {JobId} completed. Imported={Imported} Failed={Failed} Skipped={Skipped}",
                jobId,
                result.Summary?.ImportedEntries ?? 0UL,
                result.Summary?.FailedEntries ?? 0UL,
                result.Summary?.SkippedEntries ?? 0UL);
            return;
        }

        UiLogging.Warning(
            "Import job {JobId} finished with status {Status}. Message={Message} Error={Error}",
            jobId,
            result.Snapshot.Status,
            result.Snapshot.Message,
            result.Error?.Message ?? "No structured error.");
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
            : FormatTerminalSummary(summary);

        var warnings = summary?.Warnings ?? LastResult.Snapshot.Warnings;
        WarningsText = warnings.Count == 0
            ? "No warnings."
            : string.Join(Environment.NewLine, warnings);

        ErrorText = LastResult.Error is null
            ? "No error."
            : $"{LastResult.Error.Code}: {LastResult.Error.Message}";
    }

    private void ApplySnapshot(ImportJobSnapshotModel snapshot)
    {
        StatusText = snapshot.Message.Length == 0
            ? snapshot.Status.ToString()
            : snapshot.Message;
        ProgressSummaryText = FormatProgressSummary(snapshot);
    }

    private void ApplyTerminalResult(ImportJobResultModel result)
    {
        LastResult = result;
        StatusText = result.Snapshot.Message.Length == 0
            ? result.Snapshot.Status.ToString()
            : result.Snapshot.Message;
        ProgressSummaryText = FormatProgressSummary(result.Snapshot);
    }

    private static string FormatProgressSummary(ImportJobSnapshotModel snapshot)
    {
        if (snapshot.TotalEntries == 0)
        {
            return snapshot.Status switch
            {
                ImportJobStatusModel.Completed => "No supported files were found in the selected import sources.",
                ImportJobStatusModel.Failed => "No supported files were found in the selected import sources.",
                ImportJobStatusModel.Cancelled => "Import was cancelled before Librova finished processing the selected files.",
                _ => "Preparing import workload..."
            };
        }

        var processedEntries = Math.Min(snapshot.ProcessedEntries, snapshot.TotalEntries);
        var percent = Math.Clamp(snapshot.Percent, 0, 100);
        var fileNoun = snapshot.TotalEntries == 1 ? "file" : "files";
        return $"Processed {processedEntries} of {snapshot.TotalEntries} {fileNoun} ({percent}%). Imported {snapshot.ImportedEntries}, failed {snapshot.FailedEntries}, skipped {snapshot.SkippedEntries}.";
    }

    private static string FormatTerminalSummary(ImportSummaryModel summary)
    {
        if (summary.TotalEntries == 0)
        {
            return "No supported files were found in the selected import sources.";
        }

        var fileNoun = summary.TotalEntries == 1 ? "file" : "files";
        return $"Processed {summary.TotalEntries} {fileNoun}. Imported {summary.ImportedEntries}, failed {summary.FailedEntries}, skipped {summary.SkippedEntries}.";
    }

    private void OnPropertyChanged(object? sender, PropertyChangedEventArgs eventArgs)
    {
        if (eventArgs.PropertyName is nameof(IsBusy))
        {
            RaisePropertyChanged(nameof(ShowIdleImportPicker));
            RaisePropertyChanged(nameof(ShowRunningImportState));
            RaisePropertyChanged(nameof(CanAcceptNewSources));
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
        && SourcePaths.Count > 0
        && !string.IsNullOrWhiteSpace(WorkingDirectory)
        && !HasSourceValidationError
        && !HasWorkingDirectoryValidationError;

    private bool CanRefresh() => LastJobId.HasValue && !IsBusy;

    private bool CanCancel() => LastJobId.HasValue && IsBusy;

    private bool CanRemove() => LastJobId.HasValue && !IsBusy;

    private void UpdateValidationState()
    {
        SourceValidationMessage = BuildSourceValidationMessage(SourcePaths);
        WorkingDirectoryValidationMessage = BuildWorkingDirectoryValidationMessage(WorkingDirectory);
        RaisePropertyChanged(nameof(HasSourceValidationError));
        RaisePropertyChanged(nameof(HasWorkingDirectoryValidationError));
        RaisePropertyChanged(nameof(ShowSourceHelperText));
        RaisePropertyChanged(nameof(ShowWorkingDirectoryHelperText));
    }

    private static string BuildSourceValidationMessage(IReadOnlyList<string> sourcePaths)
    {
        if (sourcePaths.Count == 0)
        {
            return string.Empty;
        }

        var hasUsableSource = false;
        string? firstBlockingMessage = null;

        foreach (var sourcePath in sourcePaths)
        {
            if (string.IsNullOrWhiteSpace(sourcePath))
            {
                firstBlockingMessage ??= "Selected sources must not be blank.";
                continue;
            }

            if (!Path.IsPathFullyQualified(sourcePath))
            {
                firstBlockingMessage ??= "Use absolute source paths.";
                continue;
            }

            if (Directory.Exists(sourcePath))
            {
                hasUsableSource = true;
                continue;
            }

            if (!File.Exists(sourcePath))
            {
                firstBlockingMessage ??= "A selected source does not exist.";
                continue;
            }

            var extension = Path.GetExtension(sourcePath);
            if (!extension.Equals(".fb2", StringComparison.OrdinalIgnoreCase)
                && !extension.Equals(".epub", StringComparison.OrdinalIgnoreCase)
                && !extension.Equals(".zip", StringComparison.OrdinalIgnoreCase))
            {
                firstBlockingMessage ??= "Supported source types are .fb2, .epub, and .zip, or a directory containing them.";
                continue;
            }

            hasUsableSource = true;
        }

        return hasUsableSource ? string.Empty : firstBlockingMessage ?? string.Empty;
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

    private void SetSourcePaths(IReadOnlyList<string> sourcePaths)
    {
        SourcePaths = sourcePaths
            .Where(path => !string.IsNullOrWhiteSpace(path))
            .Select(path => path.Trim())
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToArray();
    }
}
