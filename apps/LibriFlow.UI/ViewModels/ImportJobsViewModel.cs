using LibriFlow.UI.ImportJobs;
using LibriFlow.UI.Mvvm;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace LibriFlow.UI.ViewModels;

internal sealed class ImportJobsViewModel : ObservableObject
{
    private readonly IImportJobsService _importJobsService;
    private string _sourcePath = string.Empty;
    private string _workingDirectory = string.Empty;
    private string _statusText = "Idle";
    private bool _isBusy;
    private ulong? _lastJobId;
    private ImportJobResultModel? _lastResult;
    private CancellationTokenSource? _activeImportCancellation;

    public ImportJobsViewModel(IImportJobsService importJobsService)
    {
        _importJobsService = importJobsService;
        StartImportCommand = new AsyncCommand(StartImportAsync, CanStartImport, HandleCommandErrorAsync);
        RefreshCommand = new AsyncCommand(RefreshCurrentAsync, CanRefresh, HandleCommandErrorAsync);
        CancelImportCommand = new AsyncCommand(CancelCurrentAsync, CanCancel, HandleCommandErrorAsync);
    }

    public string SourcePath
    {
        get => _sourcePath;
        set
        {
            if (SetProperty(ref _sourcePath, value))
            {
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
                StartImportCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public string StatusText
    {
        get => _statusText;
        private set => SetProperty(ref _statusText, value);
    }

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
            }
        }
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
            }
        }
    }

    public ImportJobResultModel? LastResult
    {
        get => _lastResult;
        private set => SetProperty(ref _lastResult, value);
    }

    public AsyncCommand StartImportCommand { get; }
    public AsyncCommand RefreshCommand { get; }
    public AsyncCommand CancelImportCommand { get; }

    public async Task StartImportAsync()
    {
        IsBusy = true;
        StatusText = "Starting import...";
        LastResult = null;
        _activeImportCancellation?.Dispose();
        _activeImportCancellation = new CancellationTokenSource(TimeSpan.FromSeconds(15));

        try
        {
            var jobId = await _importJobsService.StartAsync(
                new StartImportRequestModel
                {
                    SourcePath = SourcePath,
                    WorkingDirectory = WorkingDirectory
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
        StatusText = error.Message.Length == 0 ? "Import failed." : error.Message;
        return Task.CompletedTask;
    }

    private bool CanStartImport() =>
        !IsBusy
        && !string.IsNullOrWhiteSpace(SourcePath)
        && !string.IsNullOrWhiteSpace(WorkingDirectory);

    private bool CanRefresh() => LastJobId.HasValue && !IsBusy;

    private bool CanCancel() => LastJobId.HasValue && IsBusy;
}
