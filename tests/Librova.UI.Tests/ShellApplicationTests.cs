using Librova.UI.CoreHost;
using Librova.UI.Desktop;
using Librova.UI.ImportJobs;
using Librova.UI.LibraryCatalog;
using Librova.UI.Shell;
using Librova.UI.ViewModels;
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
        Assert.Equal($"Version {Librova.UI.Runtime.ApplicationVersion.Value}", application.Shell.ApplicationVersionText);
        Assert.Equal(Librova.UI.Runtime.ApplicationVersion.Author, application.Shell.ApplicationAuthorText);
        Assert.Equal(Librova.UI.Runtime.ApplicationVersion.ContactEmail, application.Shell.ApplicationContactEmailText);
        Assert.Equal(
            Path.Combine(@"C:\Libraries\Librova", "Temp", "UiImport"),
            application.Shell.ImportJobs.WorkingDirectory);
        Assert.False(application.Shell.ImportJobs.ShowForceEpubConversionOption);
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
        Assert.Equal("Library: 0 books, 0.00 MB", application.Shell.CurrentLibraryStatisticsText);
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
        var statisticsNotifications = 0;
        application.Shell.PropertyChanged += (_, eventArgs) =>
        {
            if (eventArgs.PropertyName is nameof(ShellViewModel.CurrentLibraryStatisticsText))
            {
                statisticsNotifications++;
            }
        };

        await application.InitializeAsync();
        Assert.Empty(application.Shell.LibraryBrowser.Books);

        application.Shell.ImportJobs.SourcePath = CreateTempFile(".fb2");
        application.Shell.ImportJobs.WorkingDirectory = Path.Combine(Path.GetTempPath(), "librova-shell-refresh", $"{Guid.NewGuid():N}");
        await application.Shell.ImportJobs.StartImportAsync();

        Assert.Equal(2, catalogService.ListCalls);
        Assert.Single(application.Shell.LibraryBrowser.Books);
        Assert.Equal("Fresh Import", application.Shell.LibraryBrowser.Books[0].Title);
        Assert.True(statisticsNotifications > 0);
    }

    [Fact]
    public async Task Shell_DisablesNavigationWhileImportIsRunning()
    {
        var importService = new BlockingImportJobsService();
        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\Librova"
            },
            importService,
            new FakeLibraryCatalogService());
        var application = ShellApplication.Create(
            session,
            stateStore: CreateIsolatedStateStore(),
            preferencesStore: new FakePreferencesStore());

        application.Shell.ImportJobs.SourcePath = CreateTempFile(".fb2");
        application.Shell.ImportJobs.WorkingDirectory = Path.Combine(Path.GetTempPath(), "librova-shell-running", $"{Guid.NewGuid():N}");

        var runTask = application.Shell.ImportJobs.StartImportAsync();
        await importService.WaitForPollingAsync();

        Assert.True(application.Shell.IsImportInProgress);
        Assert.True(application.Shell.IsImportSectionActive);
        Assert.False(application.Shell.ShowLibrarySectionCommand.CanExecute(null));
        Assert.False(application.Shell.ShowSettingsSectionCommand.CanExecute(null));

        await application.Shell.ImportJobs.CancelCurrentAsync();
        await runTask;

        Assert.False(application.Shell.IsImportInProgress);
        Assert.True(application.Shell.ShowLibrarySectionCommand.CanExecute(null));
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
                SelectedSourcePaths = [@"C:\Incoming\book.fb2"],
                SelectedWorkingDirectory = @"C:\Temp\Librova\Work"
            },
            stateStore: CreateIsolatedStateStore(),
            preferencesStore: new FakePreferencesStore());

        await application.Shell.ImportJobs.BrowseSourceAsync();
        await application.Shell.ImportJobs.BrowseWorkingDirectoryAsync();

        Assert.Equal([@"C:\Incoming\book.fb2"], application.Shell.ImportJobs.SourcePaths);
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
                InitialSourcePaths = [@"C:\Incoming\opened.fb2"]
            },
            stateStore: CreateIsolatedStateStore(),
            preferencesStore: new FakePreferencesStore());

        Assert.Equal([@"C:\Incoming\opened.fb2"], application.Shell.ImportJobs.SourcePaths);
    }

    [Fact]
    public void Create_WithSavedState_LoadsPreviousImportShellValues()
    {
        var stateStore = CreateIsolatedStateStore();
        stateStore.Save(new ShellStateSnapshot
        {
            SourcePaths = [@"C:\Saved\previous.fb2", @"C:\Saved\previous.epub"],
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

        Assert.Equal([@"C:\Saved\previous.fb2", @"C:\Saved\previous.epub"], application.Shell.ImportJobs.SourcePaths);
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
        Assert.NotNull(snapshot!.SourcePaths);
        Assert.Equal([@"C:\Incoming\persist.fb2"], snapshot!.SourcePaths);
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
    public async Task ShellSettings_CanSaveBuiltInConverterPreferences()
    {
        var preferencesStore = new FakePreferencesStore();
        var reloadCalls = 0;
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
            preferencesStore: preferencesStore,
            reloadShellAsync: () =>
            {
                reloadCalls++;
                return Task.CompletedTask;
            });
        application.Shell.Fb2CngExecutablePath = @"D:\Tools\fbc.exe";
        application.Shell.ImportJobs.ForceEpubConversionOnImport = true;

        await application.Shell.SavePreferencesCommand.ExecuteAsyncForTests();

        Assert.NotNull(preferencesStore.LastSavedSnapshot);
        Assert.Equal(UiConverterMode.BuiltInFb2Cng, preferencesStore.LastSavedSnapshot.ConverterMode);
        Assert.Equal(@"C:\Libraries\Librova", preferencesStore.LastSavedSnapshot.PreferredLibraryRoot);
        Assert.Equal(@"D:\Tools\fbc.exe", preferencesStore.LastSavedSnapshot.Fb2CngExecutablePath);
        Assert.False(preferencesStore.LastSavedSnapshot.ForceEpubConversionOnImport);
        Assert.Equal(1, reloadCalls);
    }

    [Fact]
    public async Task ShellSettings_ClearsForcedImportConversionWhenConverterPathIsEmpty()
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
        application.Shell.ImportJobs.ForceEpubConversionOnImport = true;

        await application.Shell.SavePreferencesCommand.ExecuteAsyncForTests();

        Assert.NotNull(preferencesStore.LastSavedSnapshot);
        Assert.Equal(UiConverterMode.Disabled, preferencesStore.LastSavedSnapshot!.ConverterMode);
        Assert.Null(preferencesStore.LastSavedSnapshot.Fb2CngExecutablePath);
        Assert.False(preferencesStore.LastSavedSnapshot.ForceEpubConversionOnImport);
    }

    [Fact]
    public void Create_DoesNotRestoreForcedImportConversionWithoutConfiguredConverter()
    {
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
            preferencesStore: new FakePreferencesStore(),
            savedPreferencesOverride: new UiPreferencesSnapshot
            {
                ForceEpubConversionOnImport = true
            });

        Assert.False(application.Shell.ImportJobs.ShowForceEpubConversionOption);
        Assert.False(application.Shell.ImportJobs.ForceEpubConversionOnImport);
    }

    [Fact]
    public void Create_WithConfiguredConverterAndSavedPreferences_EnablesForcedImportConversionOption()
    {
        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\Librova",
                ConverterMode = UiConverterMode.BuiltInFb2Cng,
                Fb2CngExecutablePath = @"C:\Tools\fbc.exe"
            },
            new FakeImportJobsService(),
            new FakeLibraryCatalogService());

        var application = ShellApplication.Create(
            session,
            stateStore: CreateIsolatedStateStore(),
            preferencesStore: new FakePreferencesStore(),
            savedPreferencesOverride: new UiPreferencesSnapshot
            {
                ForceEpubConversionOnImport = true
            });

        Assert.True(application.Shell.ImportJobs.ShowForceEpubConversionOption);
        Assert.True(application.Shell.ImportJobs.ForceEpubConversionOnImport);
    }

    [Fact]
    public async Task ShellSettings_BrowseFb2CngExecutablePathCommand_AppliesSelectedExecutable()
    {
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
            new FakePathSelectionService
            {
                SelectedExecutablePath = @"C:\Tools\fbc.exe"
            },
            stateStore: CreateIsolatedStateStore(),
            preferencesStore: new FakePreferencesStore());

        await application.Shell.BrowseFb2CngExecutablePathCommand.ExecuteAsyncForTests();

        Assert.Equal(@"C:\Tools\fbc.exe", application.Shell.Fb2CngExecutablePath);
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
        Assert.Equal(Path.Combine(@"C:\Libraries\Librova", "Logs", "ui.log"), application.Shell.UiLogFilePath);
        Assert.Contains("UI log", application.Shell.DiagnosticsHintText, StringComparison.OrdinalIgnoreCase);
        Assert.Contains("host log", application.Shell.DiagnosticsHintText, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public async Task Shell_CanRequestOpeningDifferentLibrary()
    {
        var selectedPath = Path.Combine(Path.GetTempPath(), "librova-ui-tests", "open-library", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Path.Combine(selectedPath, "Database"));
        Directory.CreateDirectory(Path.Combine(selectedPath, "Books"));
        Directory.CreateDirectory(Path.Combine(selectedPath, "Covers"));
        Directory.CreateDirectory(Path.Combine(selectedPath, "Logs"));
        Directory.CreateDirectory(Path.Combine(selectedPath, "Temp"));
        Directory.CreateDirectory(Path.Combine(selectedPath, "Trash"));
        File.WriteAllText(Path.Combine(selectedPath, "Database", "librova.db"), "sqlite-placeholder");
        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\Librova"
            },
            new FakeImportJobsService());
        string? switchedTo = null;
        UiLibraryOpenMode? requestedMode = null;
        var application = ShellApplication.Create(
            session,
            new FakePathSelectionService
            {
                SelectedWorkingDirectory = selectedPath
            },
            stateStore: CreateIsolatedStateStore(),
            preferencesStore: new FakePreferencesStore(),
            switchLibraryAsync: (path, mode) =>
            {
                switchedTo = path;
                requestedMode = mode;
                return Task.CompletedTask;
            });

        try
        {
            await application.Shell.OpenLibraryCommand.ExecuteAsyncForTests();

            Assert.Equal(selectedPath, switchedTo);
            Assert.Equal(UiLibraryOpenMode.OpenExisting, requestedMode);
        }
        finally
        {
            TryDeleteDirectory(selectedPath);
        }
    }

    [Fact]
    public async Task Shell_CanRequestCreatingDifferentLibrary()
    {
        var selectedPath = Path.Combine(Path.GetTempPath(), "librova-ui-tests", "create-library", Guid.NewGuid().ToString("N"));
        var session = new ShellSession(
            new CoreHostProcess(),
            new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
                PipePath = @"\\.\pipe\Librova.ShellApplication.Test",
                LibraryRoot = @"C:\Libraries\Librova"
            },
            new FakeImportJobsService());
        string? switchedTo = null;
        UiLibraryOpenMode? requestedMode = null;
        var application = ShellApplication.Create(
            session,
            new FakePathSelectionService
            {
                SelectedWorkingDirectory = selectedPath
            },
            stateStore: CreateIsolatedStateStore(),
            preferencesStore: new FakePreferencesStore(),
            switchLibraryAsync: (path, mode) =>
            {
                switchedTo = path;
                requestedMode = mode;
                return Task.CompletedTask;
            });

        await application.Shell.CreateLibraryCommand.ExecuteAsyncForTests();

        Assert.Equal(selectedPath, switchedTo);
        Assert.Equal(UiLibraryOpenMode.CreateNew, requestedMode);
    }

    [Fact]
    public async Task Shell_OpenLibrary_RejectsFolderThatIsNotManagedLibrary()
    {
        var selectedPath = Path.Combine(Path.GetTempPath(), "librova-ui-tests", "open-invalid", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(selectedPath);

        try
        {
            var switchCalls = 0;
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
                    SelectedWorkingDirectory = selectedPath
                },
                stateStore: CreateIsolatedStateStore(),
                preferencesStore: new FakePreferencesStore(),
                switchLibraryAsync: (path, mode) =>
                {
                    switchCalls++;
                    return Task.CompletedTask;
                });

            await application.Shell.OpenLibraryCommand.ExecuteAsyncForTests();

            Assert.Equal(0, switchCalls);
        }
        finally
        {
            TryDeleteDirectory(selectedPath);
        }
    }

    [Fact]
    public async Task Shell_CreateLibrary_RejectsNonEmptyFolder()
    {
        var selectedPath = Path.Combine(Path.GetTempPath(), "librova-ui-tests", "create-invalid", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(selectedPath);
        File.WriteAllText(Path.Combine(selectedPath, "existing.txt"), "occupied");

        try
        {
            var switchCalls = 0;
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
                    SelectedWorkingDirectory = selectedPath
                },
                stateStore: CreateIsolatedStateStore(),
                preferencesStore: new FakePreferencesStore(),
                switchLibraryAsync: (path, mode) =>
                {
                    switchCalls++;
                    return Task.CompletedTask;
                });

            await application.Shell.CreateLibraryCommand.ExecuteAsyncForTests();

            Assert.Equal(0, switchCalls);
        }
        finally
        {
            TryDeleteDirectory(selectedPath);
        }
    }

    private static ShellStateStore CreateIsolatedStateStore() =>
        new(Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}", "ui-shell-state.json"));

    private static void TryDeleteDirectory(string path)
    {
        try
        {
            if (Directory.Exists(path))
            {
                Directory.Delete(path, recursive: true);
            }
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }

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

    private sealed class BlockingImportJobsService : IImportJobsService
    {
        private readonly TaskCompletionSource _polling = new();
        private volatile bool _cancelled;

        public Task WaitForPollingAsync() => _polling.Task;

        public Task<bool> CancelAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            _cancelled = true;
            return Task.FromResult(true);
        }

        public Task<ImportJobResultModel?> TryGetResultAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobResultModel?>(_cancelled
                ? new ImportJobResultModel
                {
                    Snapshot = new ImportJobSnapshotModel
                    {
                        JobId = jobId,
                        Status = ImportJobStatusModel.Cancelled,
                        Message = "Cancelled"
                    }
                }
                : null);

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
            => Task.FromResult(11UL);

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
            => Task.FromResult(true);
    }

    private sealed class FakeLibraryCatalogService : ILibraryCatalogService
    {
        public int ListCalls { get; private set; }
        public IReadOnlyList<BookListItemModel> Items { get; init; } = [];
        public LibraryStatisticsModel Statistics { get; init; } = new();

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            ListCalls++;
            return Task.FromResult(new BookListPageModel
            {
                TotalCount = (ulong)Items.Count,
                Items = Items
            });
        }

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<LibraryStatisticsModel> GetLibraryStatisticsAsync(TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(Statistics);

        public Task<string?> ExportBookAsync(
            long bookId,
            string destinationPath,
            BookFormatModel? exportFormat,
            TimeSpan timeout,
            CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.RecycleBin
            });
    }

    private sealed class SequencedLibraryCatalogService : ILibraryCatalogService
    {
        private readonly Queue<IReadOnlyList<BookListItemModel>> _responses;
        private readonly LibraryStatisticsModel _statistics;

        public SequencedLibraryCatalogService(params IReadOnlyList<BookListItemModel>[] responses)
            : this(new LibraryStatisticsModel(), responses)
        {
        }

        public SequencedLibraryCatalogService(LibraryStatisticsModel statistics, params IReadOnlyList<BookListItemModel>[] responses)
        {
            _statistics = statistics;
            _responses = new Queue<IReadOnlyList<BookListItemModel>>(responses);
        }

        public int ListCalls { get; private set; }

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            ListCalls++;
            if (_responses.Count == 0)
            {
                return Task.FromResult(new BookListPageModel());
            }

            var items = _responses.Dequeue();
            return Task.FromResult(new BookListPageModel
            {
                TotalCount = (ulong)items.Count,
                Items = items
            });
        }

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<LibraryStatisticsModel> GetLibraryStatisticsAsync(TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(_statistics);

        public Task<string?> ExportBookAsync(
            long bookId,
            string destinationPath,
            BookFormatModel? exportFormat,
            TimeSpan timeout,
            CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.RecycleBin
            });
    }

    private sealed class FakePathSelectionService : IPathSelectionService
    {
        public IReadOnlyList<string> SelectedSourcePaths { get; init; } = [];
        public string? SelectedWorkingDirectory { get; init; }
        public string? SelectedExecutablePath { get; init; }
        public string? SelectedExportPath { get; init; }

        public Task<IReadOnlyList<string>> PickSourceFilesAsync(CancellationToken cancellationToken)
            => Task.FromResult(SelectedSourcePaths);

        public Task<string?> PickSourceDirectoryAsync(CancellationToken cancellationToken)
            => Task.FromResult<string?>(null);

        public Task<string?> PickWorkingDirectoryAsync(CancellationToken cancellationToken)
            => Task.FromResult(SelectedWorkingDirectory);

        public Task<string?> PickExecutableFileAsync(string title, CancellationToken cancellationToken)
            => Task.FromResult(SelectedExecutablePath);

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
