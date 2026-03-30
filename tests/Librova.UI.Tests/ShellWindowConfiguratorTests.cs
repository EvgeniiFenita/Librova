using Librova.UI.CoreHost;
using Librova.UI.ImportJobs;
using Librova.UI.Shell;
using Librova.UI.ViewModels;
using Xunit;

namespace Librova.UI.Tests;

public sealed class ShellWindowConfiguratorTests
{
    [Fact]
    public void CreateState_ProducesExpectedWindowTitleAndViewModel()
    {
        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellWindow.Test",
                LibraryRoot = @"C:\Libraries\Librova"
            },
            new FakeImportJobsService());
        var application = ShellApplication.Create(
            session,
            stateStore: new ShellStateStore(Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}", "ui-shell-state.json")));

        var state = ShellWindowConfigurator.CreateState(application);

        Assert.Equal("Librova", state.ViewModel.Title);
        Assert.True(state.ViewModel.HasShell);
        Assert.False(state.ViewModel.HasStartupError);
        Assert.Same(application.Shell, state.ViewModel.Shell);
    }

    [Fact]
    public void CreateStartingUpState_ProducesLoadingViewModel()
    {
        var state = ShellWindowConfigurator.CreateStartingUpState();

        Assert.Equal("Librova", state.ViewModel.Title);
        Assert.False(state.ViewModel.HasShell);
        Assert.False(state.ViewModel.HasStartupError);
        Assert.True(state.ViewModel.IsStartingUp);
        Assert.Contains("Starting", state.ViewModel.StatusText, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void CreateStartupErrorState_ProducesStartupErrorViewModel()
    {
        var state = ShellWindowConfigurator.CreateStartupErrorState("pipe startup failed");

        Assert.Equal("Librova Startup Error", state.ViewModel.Title);
        Assert.False(state.ViewModel.HasShell);
        Assert.True(state.ViewModel.HasStartupError);
        Assert.Equal("pipe startup failed", state.ViewModel.StartupError);
        Assert.NotEmpty(state.ViewModel.UiLogFilePath);
        Assert.NotEmpty(state.ViewModel.UiStateFilePath);
        Assert.NotEmpty(state.ViewModel.UiPreferencesFilePath);
        Assert.Contains("UI log", state.ViewModel.StartupGuidanceText, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void CreateFirstRunSetupState_ProducesSetupViewModel()
    {
        var setup = new FirstRunSetupViewModel(
            @"C:\Libraries\Librova",
            null,
            null,
            _ => Task.CompletedTask);

        var state = ShellWindowConfigurator.CreateFirstRunSetupState(setup);

        Assert.Equal("Librova Setup", state.ViewModel.Title);
        Assert.True(state.ViewModel.HasSetup);
        Assert.False(state.ViewModel.HasShell);
        Assert.False(state.ViewModel.HasStartupError);
        Assert.False(state.ViewModel.IsStartingUp);
        Assert.Same(setup, state.ViewModel.Setup);
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
