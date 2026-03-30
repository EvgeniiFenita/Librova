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

    public ImportJobsViewModel(IImportJobsService importJobsService)
    {
        _importJobsService = importJobsService;
        StartImportCommand = new AsyncCommand(StartImportAsync, CanStartImport);
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
            }
        }
    }

    public ulong? LastJobId
    {
        get => _lastJobId;
        private set => SetProperty(ref _lastJobId, value);
    }

    public ImportJobResultModel? LastResult
    {
        get => _lastResult;
        private set => SetProperty(ref _lastResult, value);
    }

    public AsyncCommand StartImportCommand { get; }

    public async Task StartImportAsync()
    {
        IsBusy = true;
        StatusText = "Starting import...";
        LastResult = null;

        try
        {
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(15));

            var jobId = await _importJobsService.StartAsync(
                new StartImportRequestModel
                {
                    SourcePath = SourcePath,
                    WorkingDirectory = WorkingDirectory
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token).ConfigureAwait(false);

            LastJobId = jobId;
            StatusText = $"Import job {jobId} started.";

            _ = Task.Run(() => RefreshAsync(jobId), CancellationToken.None);
        }
        finally
        {
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
            cancellation.Token).ConfigureAwait(false);

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
            cancellation.Token).ConfigureAwait(false);

        if (snapshot is not null)
        {
            StatusText = snapshot.Message.Length == 0
                ? snapshot.Status.ToString()
                : snapshot.Message;
        }
    }

    private bool CanStartImport() =>
        !IsBusy
        && !string.IsNullOrWhiteSpace(SourcePath)
        && !string.IsNullOrWhiteSpace(WorkingDirectory);
}
