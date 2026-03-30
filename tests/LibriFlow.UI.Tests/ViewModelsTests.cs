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
    }

    private sealed class FakeImportJobsService : IImportJobsService
    {
        public int TryGetSnapshotCalls { get; private set; }
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
                        ImportedEntries = 1
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
            => Task.FromResult(true);

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
}
