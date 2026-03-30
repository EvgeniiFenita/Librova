using Librova.UI.CoreHost;
using Librova.UI.Desktop;
using Librova.UI.ImportJobs;
using Librova.UI.LibraryCatalog;
using Librova.UI.Shell;
using Xunit;

namespace Librova.UI.Tests;

public sealed class ShellApplicationTests
{
    [Fact]
    public void Create_BuildsShellViewModelFromSession()
    {
        var catalogService = new FakeLibraryCatalogService();
        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\Librova"
            },
            new FakeImportJobsService(),
            catalogService);

        var application = ShellApplication.Create(
            session,
            stateStore: CreateIsolatedStateStore(),
            preferencesStore: new FakePreferencesStore());

        Assert.Equal(@"C:\Libraries\Librova", application.Shell.LibraryRoot);
        Assert.Equal(@"\\.\pipe\Librova.ShellApplication.Test", application.Shell.PipePath);
        Assert.NotNull(application.Shell.ImportJobs);
        Assert.NotNull(application.Shell.LibraryBrowser);
        Assert.True(application.Shell.IsLibrarySectionActive);
        Assert.Equal("Library", application.Shell.CurrentSectionTitle);
        Assert.Equal(
            Path.Combine(@"C:\Libraries\Librova", "Temp", "UiImport"),
            application.Shell.ImportJobs.WorkingDirectory);
    }

    [Fact]
    public async Task Shell_CanSwitchBetweenLibraryImportAndSettingsSections()
    {
        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\Librova"
            },
            new FakeImportJobsService(),
            new FakeLibraryCatalogService());
        var application = ShellApplication.Create(
            session,
            stateStore: CreateIsolatedStateStore(),
            preferencesStore: new FakePreferencesStore());

        await application.Shell.ShowImportSectionCommand.ExecuteAsyncForTests();
        Assert.True(application.Shell.IsImportSectionActive);
        Assert.Equal("Import", application.Shell.CurrentSectionTitle);

        await application.Shell.ShowSettingsSectionCommand.ExecuteAsyncForTests();
        Assert.True(application.Shell.IsSettingsSectionActive);
        Assert.Equal("Settings", application.Shell.CurrentSectionTitle);

        await application.Shell.ShowLibrarySectionCommand.ExecuteAsyncForTests();
        Assert.True(application.Shell.IsLibrarySectionActive);
        Assert.Equal("Library", application.Shell.CurrentSectionTitle);
    }

    [Fact]
    public async Task InitializeAsync_PreloadsLibraryBrowser()
    {
        var catalogService = new FakeLibraryCatalogService
        {
            Items = [
                new BookListItemModel
                {
                    BookId = 1,
                    Title = "Roadside Picnic",
                    Authors = ["Arkady Strugatsky"],
                    Language = "en",
                    Format = BookFormatModel.Epub,
                    ManagedPath = "Books/0000000001/book.epub",
                    AddedAtUtc = DateTimeOffset.UtcNow
                }
            ]
        };

        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\Librova"
            },
            new FakeImportJobsService(),
            catalogService);

        var application = ShellApplication.Create(
            session,
            stateStore: CreateIsolatedStateStore(),
            preferencesStore: new FakePreferencesStore());

        await application.InitializeAsync();

        Assert.Equal(1, catalogService.ListCalls);
        Assert.Single(application.Shell.LibraryBrowser.Books);
    }

    [Fact]
    public async Task Shell_RefreshesLibraryBrowserAfterSuccessfulImport()
    {
        var catalogService = new SequencedLibraryCatalogService(
            [],
            [
                new BookListItemModel
                {
                    BookId = 7,
                    Title = "Fresh Import",
                    Authors = ["Author"],
                    Language = "en",
                    Format = BookFormatModel.Epub,
                    ManagedPath = "Books/0000000007/fresh.epub",
                    AddedAtUtc = DateTimeOffset.UtcNow
                }
            ]);

        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\Librova"
            },
            new FakeImportJobsService(),
            catalogService);
        var application = ShellApplication.Create(
            session,
            stateStore: CreateIsolatedStateStore(),
            preferencesStore: new FakePreferencesStore());

        await application.InitializeAsync();
        Assert.Empty(application.Shell.LibraryBrowser.Books);

        application.Shell.ImportJobs.SourcePath = CreateTempFile(".fb2");
        application.Shell.ImportJobs.WorkingDirectory = Path.Combine(Path.GetTempPath(), "librova-shell-refresh", $"{Guid.NewGuid():N}");
        await application.Shell.ImportJobs.StartImportAsync();

        Assert.Equal(2, catalogService.ListCalls);
        Assert.Single(application.Shell.LibraryBrowser.Books);
        Assert.Equal("Fresh Import", application.Shell.LibraryBrowser.Books[0].Title);
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
            new FakeImportJobsService(),
            new FakeLibraryCatalogService());
        var application = ShellApplication.Create(
            session,
            new FakePathSelectionService
            {
                SelectedSourcePath = @"C:\Incoming\book.fb2",
                SelectedWorkingDirectory = @"C:\Temp\Librova\Work"
            },
            stateStore: CreateIsolatedStateStore(),
            preferencesStore: new FakePreferencesStore());

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
            new FakeImportJobsService(),
            new FakeLibraryCatalogService());

        var application = ShellApplication.Create(
            session,
            launchOptions: new ShellLaunchOptions
            {
                InitialSourcePath = @"C:\Incoming\opened.fb2"
            },
            stateStore: CreateIsolatedStateStore(),
            preferencesStore: new FakePreferencesStore());

        Assert.Equal(@"C:\Incoming\opened.fb2", application.Shell.ImportJobs.SourcePath);
        Assert.Contains("launch arguments", application.Shell.OperationalWarningsText, StringComparison.OrdinalIgnoreCase);
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
            new FakeImportJobsService(),
            new FakeLibraryCatalogService());

        var application = ShellApplication.Create(
            session,
            stateStore: stateStore,
            preferencesStore: new FakePreferencesStore());

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
            new FakeImportJobsService(),
            new FakeLibraryCatalogService());
        var application = ShellApplication.Create(
            session,
            stateStore: stateStore,
            preferencesStore: new FakePreferencesStore());
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

    [Fact]
    public async Task DisposeAsync_StillDisposesSessionWhenStateSaveFails()
    {
        var hostLifetime = new FakeAsyncDisposable();
        var session = new ShellSession(
            hostLifetime,
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\Librova"
            },
            new FakeImportJobsService(),
            new FakeLibraryCatalogService());
        var application = ShellApplication.Create(
            session,
            stateStore: new ThrowingStateStore(),
            preferencesStore: new FakePreferencesStore());

        await application.DisposeAsync();

        Assert.True(hostLifetime.DisposeCalls > 0);
    }

    [Fact]
    public void Create_WithSavedPreferences_LoadsPreferredLibraryRoot()
    {
        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\Librova"
            },
            new FakeImportJobsService(),
            new FakeLibraryCatalogService());

        var application = ShellApplication.Create(
            session,
            stateStore: CreateIsolatedStateStore(),
            preferencesStore: new FakePreferencesStore
            {
                Snapshot = new UiPreferencesSnapshot
                {
                    PreferredLibraryRoot = @"D:\Librova\SecondLibrary"
                }
            });

        Assert.Equal(@"D:\Librova\SecondLibrary", application.Shell.PreferredLibraryRoot);
        Assert.True(application.Shell.HasOperationalWarnings);
        Assert.Contains("Restart the app", application.Shell.OperationalWarningsText, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public async Task ShellSettings_CanSaveCustomConverterPreferences()
    {
        var preferencesStore = new FakePreferencesStore();
        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\Librova",
                ConverterMode = UiConverterMode.Disabled
            },
            new FakeImportJobsService(),
            new FakeLibraryCatalogService());

        var application = ShellApplication.Create(
            session,
            stateStore: CreateIsolatedStateStore(),
            preferencesStore: preferencesStore);
        application.Shell.SelectedConverterMode = UiConverterMode.CustomCommand;
        application.Shell.CustomConverterExecutablePath = @"D:\Tools\custom.exe";
        application.Shell.CustomConverterArgumentsText = "--input" + Environment.NewLine + "{source}";
        application.Shell.SelectedCustomConverterOutputMode = UiConverterOutputMode.ExactDestinationPath;

        await application.Shell.SavePreferencesCommand.ExecuteAsyncForTests();

        Assert.Equal(UiConverterMode.CustomCommand, preferencesStore.LastSavedSnapshot?.ConverterMode);
        Assert.Equal(@"D:\Tools\custom.exe", preferencesStore.LastSavedSnapshot?.CustomConverterExecutablePath);
        Assert.NotNull(preferencesStore.LastSavedSnapshot?.CustomConverterArguments);
        Assert.Equal(["--input", "{source}"], preferencesStore.LastSavedSnapshot!.CustomConverterArguments);
    }

    [Fact]
    public void Create_ExposesDiagnosticsPathsForCurrentSession()
    {
        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\Librova"
            },
            new FakeImportJobsService(),
            new FakeLibraryCatalogService());

        var application = ShellApplication.Create(
            session,
            stateStore: CreateIsolatedStateStore(),
            preferencesStore: new FakePreferencesStore());

        Assert.Equal(@"C:\Tools\LibrovaCoreHostApp.exe", application.Shell.HostExecutablePath);
        Assert.Equal(Path.Combine(@"C:\Libraries\Librova", "Logs", "host.log"), application.Shell.HostLogFilePath);
        Assert.Contains("UI log", application.Shell.DiagnosticsHintText, StringComparison.OrdinalIgnoreCase);
        Assert.Contains("host log", application.Shell.DiagnosticsHintText, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void Create_WithoutLaunchOrPreferenceMismatch_HasNoOperationalWarnings()
    {
        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\Librova"
            },
            new FakeImportJobsService(),
            new FakeLibraryCatalogService());

        var application = ShellApplication.Create(
            session,
            stateStore: CreateIsolatedStateStore(),
            preferencesStore: new FakePreferencesStore
            {
                Snapshot = new UiPreferencesSnapshot
                {
                    PreferredLibraryRoot = @"C:\Libraries\Librova"
                }
            });

        Assert.False(application.Shell.HasOperationalWarnings);
        Assert.Equal(string.Empty, application.Shell.OperationalWarningsText);
    }

    [Fact]
    public async Task ShellSettings_CanSaveAndResetPreferredLibraryRoot()
    {
        var preferencesStore = new FakePreferencesStore();
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
                SelectedWorkingDirectory = @"D:\Librova\Preferred"
            },
            stateStore: CreateIsolatedStateStore(),
            preferencesStore: preferencesStore);

        await application.Shell.BrowsePreferredLibraryRootCommand.ExecuteAsyncForTests();
        await application.Shell.SavePreferencesCommand.ExecuteAsyncForTests();

        Assert.Equal(@"D:\Librova\Preferred", preferencesStore.LastSavedSnapshot?.PreferredLibraryRoot);
        Assert.Contains("Next app launch", application.Shell.PreferencesStatusText, StringComparison.OrdinalIgnoreCase);

        await application.Shell.ResetPreferencesCommand.ExecuteAsyncForTests();

        Assert.True(preferencesStore.Cleared);
        Assert.Equal(@"C:\Libraries\Librova", application.Shell.PreferredLibraryRoot);
    }

    private static ShellStateStore CreateIsolatedStateStore() =>
        new(Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}", "ui-shell-state.json"));

    private sealed class ThrowingStateStore : IShellStateStore
    {
        public ShellStateSnapshot? TryLoad() => null;

        public void Save(ShellStateSnapshot snapshot) =>
            throw new IOException("simulated save failure");
    }

    private sealed class FakePreferencesStore : IUiPreferencesStore
    {
        public UiPreferencesSnapshot? Snapshot { get; init; }
        public UiPreferencesSnapshot? LastSavedSnapshot { get; private set; }
        public bool Cleared { get; private set; }

        public UiPreferencesSnapshot? TryLoad() => Snapshot;

        public void Save(UiPreferencesSnapshot snapshot)
        {
            LastSavedSnapshot = snapshot;
        }

        public void Clear()
        {
            Cleared = true;
        }
    }

    private sealed class FakeAsyncDisposable : IAsyncDisposable
    {
        public int DisposeCalls { get; private set; }

        public ValueTask DisposeAsync()
        {
            DisposeCalls++;
            return ValueTask.CompletedTask;
        }
    }

    private sealed class FakeImportJobsService : IImportJobsService
    {
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
                        SkippedEntries = 0
                    }
                }
                : null);

        public Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
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
        {
            _resultReady = true;
            return Task.FromResult(1UL);
        }

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
            => Task.FromResult(true);
    }

    private sealed class FakeLibraryCatalogService : ILibraryCatalogService
    {
        public int ListCalls { get; private set; }
        public IReadOnlyList<BookListItemModel> Items { get; init; } = [];

        public Task<IReadOnlyList<BookListItemModel>> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            ListCalls++;
            return Task.FromResult(Items);
        }

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(long bookId, string destinationPath, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<bool> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);
    }

    private sealed class SequencedLibraryCatalogService : ILibraryCatalogService
    {
        private readonly Queue<IReadOnlyList<BookListItemModel>> _responses;

        public SequencedLibraryCatalogService(params IReadOnlyList<BookListItemModel>[] responses)
        {
            _responses = new Queue<IReadOnlyList<BookListItemModel>>(responses);
        }

        public int ListCalls { get; private set; }

        public Task<IReadOnlyList<BookListItemModel>> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            ListCalls++;
            if (_responses.Count == 0)
            {
                return Task.FromResult<IReadOnlyList<BookListItemModel>>([]);
            }

            return Task.FromResult(_responses.Dequeue());
        }

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(long bookId, string destinationPath, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<bool> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);
    }

    private sealed class FakePathSelectionService : IPathSelectionService
    {
        public string? SelectedSourcePath { get; init; }
        public string? SelectedWorkingDirectory { get; init; }
        public string? SelectedExportPath { get; init; }

        public Task<string?> PickSourceFileAsync(CancellationToken cancellationToken)
            => Task.FromResult(SelectedSourcePath);

        public Task<string?> PickWorkingDirectoryAsync(CancellationToken cancellationToken)
            => Task.FromResult(SelectedWorkingDirectory);

        public Task<string?> PickExportDestinationAsync(string suggestedFileName, CancellationToken cancellationToken)
            => Task.FromResult(SelectedExportPath);
    }

    private static string CreateTempFile(string extension)
    {
        var directory = Path.Combine(Path.GetTempPath(), "librova-shell-tests", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        var path = Path.Combine(directory, "book" + extension);
        File.WriteAllText(path, "content");
        return path;
    }
}
