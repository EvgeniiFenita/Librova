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
    public void CreateStartupErrorState_WithRecoverySetup_ExposesRecoverySetup()
    {
        var setup = new FirstRunSetupViewModel(
            @"C:\Libraries\Librova",
            null,
            null,
            (_, _) => Task.CompletedTask);

        var state = ShellWindowConfigurator.CreateStartupErrorState("pipe startup failed", setup);

        Assert.True(state.ViewModel.HasStartupError);
        Assert.True(state.ViewModel.HasStartupRecoverySetup);
        Assert.False(state.ViewModel.IsShowingFirstRunSetup);
        Assert.Same(setup, state.ViewModel.Setup);
    }

    [Fact]
    public void CreateStartupErrorState_WithRecoverySetupForCorruptedLibrary_DisablesSamePathRetry()
    {
        var libraryRoot = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}");
        var setup = new FirstRunSetupViewModel(
            libraryRoot,
            null,
            null,
            (_, _) => Task.CompletedTask,
            requireDifferentLibraryRoot: true);

        var state = ShellWindowConfigurator.CreateStartupErrorState("database is corrupted", setup);

        Assert.True(state.ViewModel.HasStartupRecoverySetup);
        Assert.NotNull(state.ViewModel.Setup);
        Assert.True(state.ViewModel.Setup!.RequiresDifferentLibraryRoot);
        Assert.False(state.ViewModel.Setup.ContinueCommand.CanExecute(null));
    }

    [Fact]
    public void CreateFirstRunSetupState_ProducesSetupViewModel()
    {
        var setup = new FirstRunSetupViewModel(
            @"C:\Libraries\Librova",
            null,
            null,
            (_, _) => Task.CompletedTask);

        var state = ShellWindowConfigurator.CreateFirstRunSetupState(setup);

        Assert.Equal("Librova Setup", state.ViewModel.Title);
        Assert.True(state.ViewModel.HasSetup);
        Assert.False(state.ViewModel.HasShell);
        Assert.False(state.ViewModel.HasStartupError);
        Assert.True(state.ViewModel.IsShowingFirstRunSetup);
        Assert.False(state.ViewModel.IsStartingUp);
        Assert.Same(setup, state.ViewModel.Setup);
    }

    [Fact]
    public void ReloadPath_MustNotTransitionThroughHasShellFalse_PreservesCompiledBindings()
    {
        // REGRESSION GUARD for C16 (binding regression).
        //
        // When SavePreferencesCommand triggers reloadShellAsync, the implementation in
        // App.axaml.cs::ReloadCurrentShellSessionAsync transitions the window DataContext
        // Running → Running WITHOUT calling ConfigureStartingUp in between.
        //
        // ConfigureStartingUp sets Shell=null (HasShell=false), which hides the shell grid.
        // Avalonia compiled bindings on Shell.* properties do not recover even when a new
        // Running ViewModel is set afterwards — the binding chain stays broken.
        //
        // This test documents the invariant: the reload path must only produce
        // HasShell=true states on both sides of the transition.

        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellWindow.ReloadTest",
                LibraryRoot = @"C:\Libraries\Librova"
            },
            new FakeImportJobsService());
        var stateStorePath = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}", "state.json");
        var application = ShellApplication.Create(
            session,
            stateStore: new ShellStateStore(stateStorePath));

        // Before reload: Running state has HasShell=true
        var beforeReload = ShellWindowConfigurator.CreateState(application);
        Assert.True(beforeReload.ViewModel.HasShell);

        // ConfigureStartingUp WOULD produce HasShell=false — this is what breaks compiled bindings
        var startingUpState = ShellWindowConfigurator.CreateStartingUpState();
        Assert.False(startingUpState.ViewModel.HasShell);

        // After reload: the replacement Running state also has HasShell=true (no false gap)
        var afterReload = ShellWindowConfigurator.CreateState(application);
        Assert.True(afterReload.ViewModel.HasShell);
    }

    private sealed class FakeImportJobsService : IImportJobsService
    {
        public Task<bool> CancelAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ImportJobResultModel?> TryGetResultAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobResultModel?>(null);

        public Task<string> ValidateSourcesAsync(IReadOnlyList<string> sourcePaths, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(string.Empty);

        public Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobSnapshotModel?>(null);

        public Task<bool> RemoveAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ulong> StartAsync(StartImportRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(1UL);

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ImportJobResultModel> WaitForCompletionAsync(
            ulong jobId,
            TimeSpan timeout,
            TimeSpan waitTimeout,
            Action<ImportJobSnapshotModel>? onProgress,
            CancellationToken cancellationToken) =>
            Task.FromResult(new ImportJobResultModel
            {
                Snapshot = new ImportJobSnapshotModel
                {
                    JobId = jobId,
                    Status = ImportJobStatusModel.Completed,
                    Message = "Completed"
                }
            });
    }
}

