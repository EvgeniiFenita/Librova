using LibriFlow.UI.CoreHost;
using LibriFlow.UI.ImportJobs;
using LibriFlow.UI.Shell;
using Xunit;

namespace LibriFlow.UI.Tests;

public sealed class ShellApplicationTests
{
    [Fact]
    public void Create_BuildsShellViewModelFromSession()
    {
        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibriFlowCoreHostApp.exe",
                PipePath = @"\\.\pipe\LibriFlow.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\LibriFlow"
            },
            new FakeImportJobsService());

        var application = ShellApplication.Create(session);

        Assert.Equal(@"C:\Libraries\LibriFlow", application.Shell.LibraryRoot);
        Assert.Equal(@"\\.\pipe\LibriFlow.ShellApplication.Test", application.Shell.PipePath);
        Assert.NotNull(application.Shell.ImportJobs);
    }

    private sealed class FakeImportJobsService : IImportJobsService
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
            => Task.FromResult(1UL);

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
            => Task.FromResult(true);
    }
}
