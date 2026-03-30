using LibriFlow.UI.CoreHost;
using LibriFlow.UI.ImportJobs;
using LibriFlow.UI.Shell;
using Xunit;

namespace LibriFlow.UI.Tests;

public sealed class ShellWindowConfiguratorTests
{
    [Fact]
    public void CreateState_ProducesExpectedWindowTitleAndViewModel()
    {
        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibriFlowCoreHostApp.exe",
                PipePath = @"\\.\pipe\LibriFlow.ShellWindow.Test",
                LibraryRoot = @"C:\Libraries\LibriFlow"
            },
            new FakeImportJobsService());
        var application = ShellApplication.Create(session);

        var state = ShellWindowConfigurator.CreateState(application);

        Assert.Equal("LibriFlow", state.ViewModel.Title);
        Assert.True(state.ViewModel.HasShell);
        Assert.False(state.ViewModel.HasStartupError);
        Assert.Same(application.Shell, state.ViewModel.Shell);
    }

    [Fact]
    public void CreateStartupErrorState_ProducesStartupErrorViewModel()
    {
        var state = ShellWindowConfigurator.CreateStartupErrorState("pipe startup failed");

        Assert.Equal("LibriFlow Startup Error", state.ViewModel.Title);
        Assert.False(state.ViewModel.HasShell);
        Assert.True(state.ViewModel.HasStartupError);
        Assert.Equal("pipe startup failed", state.ViewModel.StartupError);
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
