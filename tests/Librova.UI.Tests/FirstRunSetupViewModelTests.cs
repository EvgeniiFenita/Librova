using Librova.UI.Desktop;
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
            _ => Task.CompletedTask);

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
            _ => Task.CompletedTask);

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
            _ => Task.CompletedTask);

        await viewModel.BrowseLibraryRootCommand.ExecuteAsyncForTests();

        Assert.Equal(selectedPath, viewModel.LibraryRoot);
        Assert.False(viewModel.HasValidationError);
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
            libraryRoot =>
            {
                continuedWith = libraryRoot;
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
            _ => Task.CompletedTask);

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
            _ => throw new InvalidOperationException("startup failed"));

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
                _ => Task.CompletedTask,
                additionalValidation: LibraryRootInspection.BuildCreateNewValidationMessage);

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
            _ => Task.CompletedTask,
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
            _ => Task.CompletedTask,
            requireDifferentLibraryRoot: true);

        viewModel.LibraryRoot = otherLibraryRoot;

        Assert.True(viewModel.ContinueCommand.CanExecute(null));
        Assert.False(viewModel.HasValidationError);
        Assert.Contains("different library root", viewModel.HelperText, StringComparison.OrdinalIgnoreCase);
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
