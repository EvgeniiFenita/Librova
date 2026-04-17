using Librova.UI.Desktop;
using Librova.UI.CoreHost;
using Librova.UI.Shell;
using Librova.UI.ViewModels;
using Xunit;

namespace Librova.UI.Tests;

public sealed class FirstRunSetupViewModelTests
{
    [Fact]
    public void ContinueCommand_DisabledForInvalidLibraryRoot()
    {
        var viewModel = new FirstRunSetupViewModel(
            "relative\\library",
            new FakePathSelectionService(),
            new FakePreferencesStore(),
            (_, _) => Task.CompletedTask);

        Assert.False(viewModel.ContinueCommand.CanExecute(null));
        Assert.True(viewModel.HasValidationError);
        Assert.Contains("absolute", viewModel.ValidationMessage, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void ContinueCommand_DisabledForUnavailableDrive()
    {
        var unavailableRoot = GetUnavailableDriveRoot();
        var viewModel = new FirstRunSetupViewModel(
            unavailableRoot,
            new FakePathSelectionService(),
            new FakePreferencesStore(),
            (_, _) => Task.CompletedTask);

        Assert.False(viewModel.ContinueCommand.CanExecute(null));
        Assert.True(viewModel.HasValidationError);
        Assert.Contains("available drive", viewModel.ValidationMessage, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public async Task BrowseLibraryRootAsync_AppliesPickedPath()
    {
        var selectedPath = BuildAvailableLibraryRoot("browse");
        var viewModel = new FirstRunSetupViewModel(
            BuildAvailableLibraryRoot("initial"),
            new FakePathSelectionService
            {
                SelectedWorkingDirectory = selectedPath
            },
            new FakePreferencesStore(),
            (_, _) => Task.CompletedTask);

        await viewModel.BrowseLibraryRootCommand.ExecuteAsyncForTests();

        Assert.Equal(selectedPath, viewModel.LibraryRoot);
        Assert.False(viewModel.HasValidationError);
    }

    [Fact]
    public async Task BrowseLibraryRootAsync_CompletesBeforeDeferredModeValidationAndContinuePerformsFinalCheck()
    {
        var selectedPath = BuildAvailableLibraryRoot("browse-deferred-validation");
        Directory.CreateDirectory(selectedPath);
        File.WriteAllText(Path.Combine(selectedPath, "existing.txt"), "occupied");
        var deferredValidation = new TaskCompletionSource<string>(TaskCreationOptions.RunContinuationsAsynchronously);
        var continueCalled = false;

        try
        {
            var viewModel = new FirstRunSetupViewModel(
                BuildAvailableLibraryRoot("initial-deferred-validation"),
                new FakePathSelectionService
                {
                    SelectedWorkingDirectory = selectedPath
                },
                new FakePreferencesStore(),
                (_, _) =>
                {
                    continueCalled = true;
                    return Task.CompletedTask;
                },
                allowModeSelection: true,
                modeValidationAsync: (_, _) => deferredValidation.Task);

            await viewModel.BrowseLibraryRootCommand.ExecuteAsyncForTests();

            Assert.Equal(selectedPath, viewModel.LibraryRoot);
            Assert.True(viewModel.BrowseLibraryRootCommand.CanExecute(null));
            Assert.True(viewModel.ContinueCommand.CanExecute(null));
            Assert.False(viewModel.HasValidationError);

            var continueTask = viewModel.ContinueCommand.ExecuteAsyncForTests();
            Assert.False(continueTask.IsCompleted);

            deferredValidation.SetResult("Create Library requires an empty target directory.");
            await continueTask;

            Assert.False(continueCalled);
            Assert.True(viewModel.HasValidationError);
            Assert.Equal("Create Library requires an empty target directory.", viewModel.ValidationMessage);
            Assert.Equal("Create Library requires an empty target directory.", viewModel.StatusText);
        }
        finally
        {
            TryDeleteDirectory(selectedPath);
        }
    }

    [Fact]
    public async Task ContinueCommand_SavesPreferenceAfterSuccessfulCallback()
    {
        var libraryRoot = BuildAvailableLibraryRoot("save-success");
        var preferences = new FakePreferencesStore();
        string? continuedWith = null;
        var viewModel = new FirstRunSetupViewModel(
            libraryRoot,
            new FakePathSelectionService(),
            preferences,
            (selectedLibraryRoot, selectedMode) =>
            {
                continuedWith = selectedLibraryRoot;
                Assert.Equal(UiLibraryOpenMode.CreateNew, selectedMode);
                return Task.CompletedTask;
            });

        await viewModel.ContinueCommand.ExecuteAsyncForTests();

        Assert.Equal(libraryRoot, preferences.LastSavedSnapshot?.PreferredLibraryRoot);
        Assert.Equal(libraryRoot, continuedWith);
    }

    [Fact]
    public async Task ContinueCommand_PreservesExistingConverterPreferences()
    {
        var libraryRoot = BuildAvailableLibraryRoot("preserve-converter");
        var preferences = new FakePreferencesStore
        {
            Snapshot = new UiPreferencesSnapshot
            {
                PreferredLibraryRoot = @"C:\Libraries\Old",
                ConverterMode = CoreHost.UiConverterMode.BuiltInFb2Cng,
                Fb2CngExecutablePath = @"C:\Tools\fbc.exe"
            }
        };

        var viewModel = new FirstRunSetupViewModel(
            libraryRoot,
            new FakePathSelectionService(),
            preferences,
            (_, _) => Task.CompletedTask);

        await viewModel.ContinueCommand.ExecuteAsyncForTests();

        Assert.Equal(libraryRoot, preferences.LastSavedSnapshot?.PreferredLibraryRoot);
        Assert.Equal(CoreHost.UiConverterMode.BuiltInFb2Cng, preferences.LastSavedSnapshot?.ConverterMode);
        Assert.Equal(@"C:\Tools\fbc.exe", preferences.LastSavedSnapshot?.Fb2CngExecutablePath);
    }

    [Fact]
    public async Task ContinueCommand_DoesNotSavePreferenceWhenCallbackFails()
    {
        var libraryRoot = BuildAvailableLibraryRoot("callback-fails");
        var preferences = new FakePreferencesStore();
        var viewModel = new FirstRunSetupViewModel(
            libraryRoot,
            new FakePathSelectionService(),
            preferences,
            (_, _) => throw new InvalidOperationException("startup failed"));

        await viewModel.ContinueCommand.ExecuteAsyncForTests();

        Assert.Null(preferences.LastSavedSnapshot);
        Assert.Contains("startup failed", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void ContinueCommand_DisabledWhenAdditionalValidationFails()
    {
        var libraryRoot = BuildAvailableLibraryRoot("non-empty-create");
        Directory.CreateDirectory(libraryRoot);
        File.WriteAllText(Path.Combine(libraryRoot, "existing.txt"), "occupied");

        try
        {
            var viewModel = new FirstRunSetupViewModel(
                libraryRoot,
                new FakePathSelectionService(),
                new FakePreferencesStore(),
                (_, _) => Task.CompletedTask,
                lockedLibraryOpenMode: UiLibraryOpenMode.CreateNew);

            Assert.False(viewModel.ContinueCommand.CanExecute(null));
            Assert.Contains("empty target directory", viewModel.ValidationMessage, StringComparison.OrdinalIgnoreCase);
        }
        finally
        {
            TryDeleteDirectory(libraryRoot);
        }
    }

    [Fact]
    public void ContinueCommand_DisabledWhenRecoveryRequiresDifferentLibraryRootAndPathIsUnchanged()
    {
        var libraryRoot = BuildAvailableLibraryRoot("same-path-retry");
        var viewModel = new FirstRunSetupViewModel(
            libraryRoot,
            new FakePathSelectionService(),
            new FakePreferencesStore(),
            (_, _) => Task.CompletedTask,
            requireDifferentLibraryRoot: true);

        Assert.False(viewModel.ContinueCommand.CanExecute(null));
        Assert.True(viewModel.HasValidationError);
        Assert.Contains("different library root", viewModel.ValidationMessage, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void ContinueCommand_EnabledWhenRecoveryRequiresDifferentLibraryRootAndPathChanges()
    {
        var initialLibraryRoot = BuildAvailableLibraryRoot("different-path-initial");
        var otherLibraryRoot = BuildAvailableLibraryRoot("different-path-target");
        var viewModel = new FirstRunSetupViewModel(
            initialLibraryRoot,
            new FakePathSelectionService(),
            new FakePreferencesStore(),
            (_, _) => Task.CompletedTask,
            requireDifferentLibraryRoot: true);

        viewModel.LibraryRoot = otherLibraryRoot;

        Assert.True(viewModel.ContinueCommand.CanExecute(null));
        Assert.False(viewModel.HasValidationError);
        Assert.Contains("different library root", viewModel.HelperText, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public async Task ContinueCommand_CanOpenExistingLibraryFromFirstRunSetup()
    {
        var libraryRoot = BuildAvailableLibraryRoot("open-existing");
        CreateManagedLibraryRoot(libraryRoot);
        var preferences = new FakePreferencesStore();
        string? continuedWith = null;
        UiLibraryOpenMode? continuedMode = null;

        try
        {
            var viewModel = new FirstRunSetupViewModel(
                libraryRoot,
                new FakePathSelectionService(),
                preferences,
                (selectedLibraryRoot, selectedMode) =>
                {
                    continuedWith = selectedLibraryRoot;
                    continuedMode = selectedMode;
                    return Task.CompletedTask;
                },
                allowModeSelection: true);

            await viewModel.SelectOpenLibraryModeCommand.ExecuteAsyncForTests();
            await viewModel.ContinueCommand.ExecuteAsyncForTests();

            Assert.Equal(libraryRoot, continuedWith);
            Assert.Equal(UiLibraryOpenMode.OpenExisting, continuedMode);
            Assert.Equal("Open Library", viewModel.ContinueButtonText);
            Assert.False(viewModel.HasValidationError);
            Assert.Equal(libraryRoot, preferences.LastSavedSnapshot?.PreferredLibraryRoot);
        }
        finally
        {
            TryDeleteDirectory(libraryRoot);
        }
    }

    [Fact]
    public async Task OpenLibraryMode_RejectsEmptyDirectoryUntilAnExistingManagedLibraryIsChosen()
    {
        var emptyDirectory = BuildAvailableLibraryRoot("first-run-empty-open");
        Directory.CreateDirectory(emptyDirectory);
        var managedLibraryRoot = BuildAvailableLibraryRoot("first-run-managed-open");
        CreateManagedLibraryRoot(managedLibraryRoot);
        var preferences = new FakePreferencesStore();
        var continued = false;

        try
        {
            var viewModel = new FirstRunSetupViewModel(
                emptyDirectory,
                new FakePathSelectionService(),
                preferences,
                (_, _) =>
                {
                    continued = true;
                    return Task.CompletedTask;
                },
                allowModeSelection: true);

            await viewModel.SelectOpenLibraryModeCommand.ExecuteAsyncForTests();

            await viewModel.ContinueCommand.ExecuteAsyncForTests();

            Assert.False(continued);
            Assert.Null(preferences.LastSavedSnapshot);
            Assert.Contains("existing Librova library", viewModel.ValidationMessage, StringComparison.OrdinalIgnoreCase);
            Assert.Contains("existing Librova library", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);

            viewModel.LibraryRoot = managedLibraryRoot;

            Assert.True(viewModel.ContinueCommand.CanExecute(null));
            Assert.False(viewModel.HasValidationError);
        }
        finally
        {
            TryDeleteDirectory(emptyDirectory);
            TryDeleteDirectory(managedLibraryRoot);
        }
    }

    [Fact]
    public async Task StaleOpenModeValidationResult_DoesNotOverrideLaterCreateModeSelection()
    {
        var emptyDirectory = BuildAvailableLibraryRoot("mode-toggle-regression");
        Directory.CreateDirectory(emptyDirectory);
        var openValidation = new TaskCompletionSource<string>(TaskCreationOptions.RunContinuationsAsynchronously);
        try
        {
            var viewModel = new FirstRunSetupViewModel(
                emptyDirectory,
                new FakePathSelectionService(),
                new FakePreferencesStore(),
                (_, _) => Task.CompletedTask,
                allowModeSelection: true,
                modeValidationAsync: (_, mode) => mode switch
                {
                    UiLibraryOpenMode.OpenExisting => openValidation.Task,
                    UiLibraryOpenMode.CreateNew => Task.FromResult(string.Empty),
                    _ => Task.FromResult(string.Empty)
                });

            // Initially on Create New with a valid empty folder — Continue must be enabled.
            Assert.True(viewModel.ContinueCommand.CanExecute(null), "Initial state: Continue must be enabled for a valid empty folder in Create New mode");

            // Switch to Open Existing; validation is still pending, so Continue remains immediately available.
            await viewModel.SelectOpenLibraryModeCommand.ExecuteAsyncForTests();
            Assert.True(viewModel.ContinueCommand.CanExecute(null), "After switching to Open Existing: deferred validation should not lock the command immediately");

            // Switch back to Create New before the stale Open Existing validation completes.
            await viewModel.SelectCreateLibraryModeCommand.ExecuteAsyncForTests();
            Assert.True(viewModel.ContinueCommand.CanExecute(null), "After switching back to Create New: Continue must be enabled again");

            openValidation.SetResult("Selected folder is not an existing Librova library.");
            await Task.Yield();

            Assert.True(viewModel.ContinueCommand.CanExecute(null), "A stale Open Existing validation result must not disable Create New");
            Assert.False(viewModel.HasValidationError);
        }
        finally
        {
            TryDeleteDirectory(emptyDirectory);
        }
    }

    private sealed class FakePathSelectionService : IPathSelectionService
    {
        public string? SelectedWorkingDirectory { get; init; }

        public Task<IReadOnlyList<string>> PickSourceFilesAsync(CancellationToken cancellationToken)
            => Task.FromResult<IReadOnlyList<string>>([]);

        public Task<string?> PickSourceDirectoryAsync(CancellationToken cancellationToken)
            => Task.FromResult<string?>(null);

        public Task<string?> PickWorkingDirectoryAsync(CancellationToken cancellationToken)
            => Task.FromResult(SelectedWorkingDirectory);

        public Task<string?> PickExecutableFileAsync(string title, CancellationToken cancellationToken)
            => Task.FromResult<string?>(null);

        public Task<string?> PickExportDestinationAsync(string suggestedFileName, CancellationToken cancellationToken)
            => Task.FromResult<string?>(null);
    }

    private sealed class FakePreferencesStore : IUiPreferencesStore
    {
        public UiPreferencesSnapshot? Snapshot { get; init; }
        public UiPreferencesSnapshot? LastSavedSnapshot { get; private set; }

        public UiPreferencesSnapshot? TryLoad() => Snapshot;

        public void Save(UiPreferencesSnapshot snapshot)
        {
            LastSavedSnapshot = snapshot;
        }

        public void Clear()
        {
        }
    }

    private static string GetUnavailableDriveRoot()
    {
        for (var drive = 'D'; drive <= 'Z'; drive++)
        {
            var candidate = $"{drive}:\\Librova";
            var root = Path.GetPathRoot(candidate);
            if (!string.IsNullOrWhiteSpace(root) && !Directory.Exists(root))
            {
                return candidate;
            }
        }

        return @"\\nonexistent-server\missing-share\Librova";
    }

    private static string BuildAvailableLibraryRoot(string scenario) =>
        Path.Combine(Path.GetTempPath(), "librova-ui-tests", scenario, Guid.NewGuid().ToString("N"));

    private static void CreateManagedLibraryRoot(string libraryRoot)
    {
        Directory.CreateDirectory(Path.Combine(libraryRoot, "Database"));
        Directory.CreateDirectory(Path.Combine(libraryRoot, "Books"));
        Directory.CreateDirectory(Path.Combine(libraryRoot, "Covers"));
        Directory.CreateDirectory(Path.Combine(libraryRoot, "Logs"));
        Directory.CreateDirectory(Path.Combine(libraryRoot, "Trash"));
        File.WriteAllText(Path.Combine(libraryRoot, "Database", "librova.db"), string.Empty);
    }

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
}
