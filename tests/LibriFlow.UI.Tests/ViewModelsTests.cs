using LibriFlow.UI.ImportJobs;
using LibriFlow.UI.ViewModels;
using Xunit;

namespace LibriFlow.UI.Tests;

public sealed class ViewModelsTests
{
    [Fact]
    public async Task ImportJobsViewModel_StartsImportAndPollsUntilTerminalResult()
    {
        var service = new FakeImportJobsService();
        var viewModel = new ImportJobsViewModel(service)
        {
            SourcePath = @"C:\Books\book.fb2",
            WorkingDirectory = @"C:\Work"
        };

        await viewModel.StartImportAsync();

        Assert.Equal(42UL, viewModel.LastJobId);
        Assert.NotNull(viewModel.LastResult);
        Assert.Equal(ImportJobStatusModel.Completed, viewModel.LastResult!.Snapshot.Status);
        Assert.Equal("Completed", viewModel.StatusText);
        Assert.Equal("Imported 1 of 1; failed 0; skipped 0.", viewModel.ResultSummaryText);
        Assert.Equal("Watch for duplicates", viewModel.WarningsText);
        Assert.Equal("No error.", viewModel.ErrorText);
        Assert.True(service.TryGetSnapshotCalls > 0);
        Assert.False(viewModel.IsBusy);
    }

    [Fact]
    public void ImportJobsViewModel_CommandRequiresPaths()
    {
        var viewModel = new ImportJobsViewModel(new FakeImportJobsService());

        Assert.False(viewModel.StartImportCommand.CanExecute(null));

        viewModel.SourcePath = @"C:\Books\book.fb2";
        viewModel.WorkingDirectory = @"C:\Work";

        Assert.True(viewModel.StartImportCommand.CanExecute(null));
    }

    [Fact]
    public async Task ImportJobsViewModel_SetsControlledErrorStateWhenImportFails()
    {
        var viewModel = new ImportJobsViewModel(new FailingImportJobsService())
        {
            SourcePath = @"C:\Books\book.fb2",
            WorkingDirectory = @"C:\Work"
        };

        await viewModel.StartImportCommand.ExecuteAsyncForTests();

        Assert.Equal("transport failed", viewModel.StatusText);
        Assert.False(viewModel.IsBusy);
        Assert.Null(viewModel.LastResult);
        Assert.Equal("No completed job yet.", viewModel.ResultSummaryText);
    }

    [Fact]
    public async Task ImportJobsViewModel_RefreshCommandLoadsCurrentSnapshot()
    {
        var service = new SnapshotOnlyImportJobsService();
        var viewModel = new ImportJobsViewModel(service)
        {
            SourcePath = @"C:\Books\book.fb2",
            WorkingDirectory = @"C:\Work"
        };

        await viewModel.StartImportAsync();
        service.ReturnTerminalResult = false;
        await viewModel.RefreshCommand.ExecuteAsyncForTests();

        Assert.Equal("Still running", viewModel.StatusText);
        Assert.True(service.RefreshSnapshotCalls > 0);
    }

    [Fact]
    public async Task ImportJobsViewModel_CancelCommandRequestsCancellationForActiveJob()
    {
        var service = new CancelAwareImportJobsService();
        var viewModel = new ImportJobsViewModel(service)
        {
            SourcePath = @"C:\Books\book.fb2",
            WorkingDirectory = @"C:\Work"
        };

        var runTask = viewModel.StartImportAsync();
        await service.WaitForStartAsync();
        await service.WaitForPollingAsync();

        Assert.True(viewModel.CancelImportCommand.CanExecute(null));

        await viewModel.CancelImportCommand.ExecuteAsyncForTests();
        Assert.Contains("cancellation requested", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
        await runTask;

        Assert.True(service.CancelCalls > 0);
        Assert.Equal(ImportJobStatusModel.Cancelled, viewModel.LastResult?.Snapshot.Status);
    }

    [Fact]
    public async Task ImportJobsViewModel_RemoveCommandClearsCurrentJobState()
    {
        var service = new FakeImportJobsService();
        var viewModel = new ImportJobsViewModel(service)
        {
            SourcePath = @"C:\Books\book.fb2",
            WorkingDirectory = @"C:\Work"
        };

        await viewModel.StartImportAsync();
        Assert.True(viewModel.RemoveJobCommand.CanExecute(null));

        await viewModel.RemoveJobCommand.ExecuteAsyncForTests();

        Assert.Equal(42UL, service.LastRemovedJobId);
        Assert.Null(viewModel.LastJobId);
        Assert.Null(viewModel.LastResult);
        Assert.Equal("No completed job yet.", viewModel.ResultSummaryText);
        Assert.Contains("was removed", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
    }

    private sealed class FakeImportJobsService : IImportJobsService
    {
        public int TryGetSnapshotCalls { get; private set; }
        public ulong? LastRemovedJobId { get; private set; }
        private bool _resultReady;

        public Task<bool> CancelAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ImportJobResultModel?> TryGetResultAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobResultModel?>(_resultReady
                ? new ImportJobResultModel
                {
                    Snapshot = new ImportJobSnapshotModel
                    {
                        JobId = jobId,
                        Status = ImportJobStatusModel.Completed,
                        Message = "Completed"
                    },
                    Summary = new ImportSummaryModel
                    {
                        Mode = ImportModeModel.SingleFile,
                        TotalEntries = 1,
                        ImportedEntries = 1,
                        FailedEntries = 0,
                        SkippedEntries = 0,
                        Warnings = ["Watch for duplicates"]
                    }
                }
                : null);

        public Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            TryGetSnapshotCalls++;
            _resultReady = true;
            return Task.FromResult<ImportJobSnapshotModel?>(new ImportJobSnapshotModel
            {
                JobId = jobId,
                Status = ImportJobStatusModel.Running,
                Message = "Running"
            });
        }

        public Task<bool> RemoveAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            LastRemovedJobId = jobId;
            return Task.FromResult(true);
        }

        public Task<ulong> StartAsync(StartImportRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(42UL);

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
            => Task.FromResult(true);
    }

    private sealed class FailingImportJobsService : IImportJobsService
    {
        public Task<bool> CancelAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ImportJobResultModel?> TryGetResultAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobResultModel?>(null);

        public Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobSnapshotModel?>(null);

        public Task<bool> RemoveAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ulong> StartAsync(StartImportRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => throw new InvalidOperationException("transport failed");

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
            => Task.FromResult(true);
    }

    private sealed class SnapshotOnlyImportJobsService : IImportJobsService
    {
        public int RefreshSnapshotCalls { get; private set; }
        public bool ReturnTerminalResult { get; set; } = true;

        public Task<bool> CancelAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ImportJobResultModel?> TryGetResultAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobResultModel?>(ReturnTerminalResult
                ? new ImportJobResultModel
                {
                    Snapshot = new ImportJobSnapshotModel
                    {
                        JobId = jobId,
                        Status = ImportJobStatusModel.Completed,
                        Message = "Completed"
                    }
                }
                : null);

        public Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            RefreshSnapshotCalls++;
            return Task.FromResult<ImportJobSnapshotModel?>(new ImportJobSnapshotModel
            {
                JobId = jobId,
                Status = ImportJobStatusModel.Running,
                Message = "Still running"
            });
        }

        public Task<bool> RemoveAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ulong> StartAsync(StartImportRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(64UL);

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
            => Task.FromResult(false);
    }

    private sealed class CancelAwareImportJobsService : IImportJobsService
    {
        private readonly TaskCompletionSource _started = new();
        private readonly TaskCompletionSource _polling = new();
        private volatile bool _cancelled;

        public int CancelCalls { get; private set; }
        public Task WaitForStartAsync() => _started.Task;
        public Task WaitForPollingAsync() => _polling.Task;

        public Task<bool> CancelAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            CancelCalls++;
            _cancelled = true;
            return Task.FromResult(true);
        }

        public Task<ImportJobResultModel?> TryGetResultAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            _started.TrySetResult();

            if (_cancelled)
            {
                return Task.FromResult<ImportJobResultModel?>(new ImportJobResultModel
                {
                    Snapshot = new ImportJobSnapshotModel
                    {
                        JobId = jobId,
                        Status = ImportJobStatusModel.Cancelled,
                        Message = "Cancelled"
                    }
                });
            }

            return Task.FromResult<ImportJobResultModel?>(null);
        }

        public Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            _polling.TrySetResult();
            return Task.FromResult<ImportJobSnapshotModel?>(new ImportJobSnapshotModel
            {
                JobId = jobId,
                Status = ImportJobStatusModel.Running,
                Message = "Running"
            });
        }

        public Task<bool> RemoveAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ulong> StartAsync(StartImportRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(77UL);

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
            => Task.FromResult(true);
    }
}
