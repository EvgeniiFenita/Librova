using Librova.UI.CoreHost;
using Librova.UI.Desktop;
using Librova.UI.ImportJobs;
using Librova.UI.Shell;
using Xunit;

namespace Librova.UI.Tests;

public sealed class ShellApplicationTests
{
    [Fact]
    public void Create_BuildsShellViewModelFromSession()
    {
        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\Librova"
            },
            new FakeImportJobsService());

        var application = ShellApplication.Create(session, stateStore: CreateIsolatedStateStore());

        Assert.Equal(@"C:\Libraries\Librova", application.Shell.LibraryRoot);
        Assert.Equal(@"\\.\pipe\Librova.ShellApplication.Test", application.Shell.PipePath);
        Assert.NotNull(application.Shell.ImportJobs);
        Assert.Equal(
            Path.Combine(@"C:\Libraries\Librova", "Temp", "UiImport"),
            application.Shell.ImportJobs.WorkingDirectory);
    }

    [Fact]
    public async Task Create_WithPathSelectionService_WiresBrowseActionsIntoShellViewModel()
    {
        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\Librova"
            },
            new FakeImportJobsService());
        var application = ShellApplication.Create(
            session,
            new FakePathSelectionService
            {
                SelectedSourcePath = @"C:\Incoming\book.fb2",
                SelectedWorkingDirectory = @"C:\Temp\Librova\Work"
            },
            stateStore: CreateIsolatedStateStore());

        await application.Shell.ImportJobs.BrowseSourceAsync();
        await application.Shell.ImportJobs.BrowseWorkingDirectoryAsync();

        Assert.Equal(@"C:\Incoming\book.fb2", application.Shell.ImportJobs.SourcePath);
        Assert.Equal(@"C:\Temp\Librova\Work", application.Shell.ImportJobs.WorkingDirectory);
    }

    [Fact]
    public void Create_WithInitialSourcePath_PrefillsImportShell()
    {
        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\Librova"
            },
            new FakeImportJobsService());

        var application = ShellApplication.Create(
            session,
            launchOptions: new ShellLaunchOptions
            {
                InitialSourcePath = @"C:\Incoming\opened.fb2"
            },
            stateStore: CreateIsolatedStateStore());

        Assert.Equal(@"C:\Incoming\opened.fb2", application.Shell.ImportJobs.SourcePath);
    }

    [Fact]
    public void Create_WithSavedState_LoadsPreviousImportShellValues()
    {
        var stateStore = CreateIsolatedStateStore();
        stateStore.Save(new ShellStateSnapshot
        {
            SourcePath = @"C:\Saved\previous.fb2",
            WorkingDirectory = @"C:\Saved\Work",
            AllowProbableDuplicates = true
        });
        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\Librova"
            },
            new FakeImportJobsService());

        var application = ShellApplication.Create(session, stateStore: stateStore);

        Assert.Equal(@"C:\Saved\previous.fb2", application.Shell.ImportJobs.SourcePath);
        Assert.Equal(@"C:\Saved\Work", application.Shell.ImportJobs.WorkingDirectory);
        Assert.True(application.Shell.ImportJobs.AllowProbableDuplicates);
    }

    [Fact]
    public async Task DisposeAsync_PersistsCurrentImportShellState()
    {
        var stateStore = CreateIsolatedStateStore();
        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\Librova"
            },
            new FakeImportJobsService());
        var application = ShellApplication.Create(session, stateStore: stateStore);
        application.Shell.ImportJobs.SourcePath = @"C:\Incoming\persist.fb2";
        application.Shell.ImportJobs.WorkingDirectory = @"C:\Temp\Persisted";
        application.Shell.ImportJobs.AllowProbableDuplicates = true;

        await application.DisposeAsync();

        var snapshot = stateStore.TryLoad();
        Assert.NotNull(snapshot);
        Assert.Equal(@"C:\Incoming\persist.fb2", snapshot!.SourcePath);
        Assert.Equal(@"C:\Temp\Persisted", snapshot.WorkingDirectory);
        Assert.True(snapshot.AllowProbableDuplicates);
    }

    private static ShellStateStore CreateIsolatedStateStore() =>
        new(Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}", "ui-shell-state.json"));

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

    private sealed class FakePathSelectionService : IPathSelectionService
    {
        public string? SelectedSourcePath { get; init; }
        public string? SelectedWorkingDirectory { get; init; }

        public Task<string?> PickSourceFileAsync(CancellationToken cancellationToken)
            => Task.FromResult(SelectedSourcePath);

        public Task<string?> PickWorkingDirectoryAsync(CancellationToken cancellationToken)
            => Task.FromResult(SelectedWorkingDirectory);
    }
}
