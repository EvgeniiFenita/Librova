using LibriFlow.UI.ImportJobs;
using LibriFlow.UI.ViewModels;
using Xunit;

namespace LibriFlow.UI.Tests;

public sealed class ViewModelsTests
{
    [Fact]
    public async Task ImportJobsViewModel_StartsImportAndRefreshesResult()
    {
        var service = new FakeImportJobsService();
        var viewModel = new ImportJobsViewModel(service)
        {
            SourcePath = @"C:\Books\book.fb2",
            WorkingDirectory = @"C:\Work"
        };

        await viewModel.StartImportAsync();
        await viewModel.RefreshAsync();

        Assert.Equal(42UL, viewModel.LastJobId);
        Assert.NotNull(viewModel.LastResult);
        Assert.Equal(ImportJobStatusModel.Completed, viewModel.LastResult!.Snapshot.Status);
        Assert.Equal("Completed", viewModel.StatusText);
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

    private sealed class FakeImportJobsService : IImportJobsService
    {
        public Task<bool> CancelAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ImportJobResultModel?> TryGetResultAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobResultModel?>(new ImportJobResultModel
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
            });

        public Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobSnapshotModel?>(new ImportJobSnapshotModel
            {
                JobId = jobId,
                Status = ImportJobStatusModel.Running,
                Message = "Running"
            });

        public Task<bool> RemoveAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ulong> StartAsync(StartImportRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(42UL);

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
            => Task.FromResult(true);
    }
}
