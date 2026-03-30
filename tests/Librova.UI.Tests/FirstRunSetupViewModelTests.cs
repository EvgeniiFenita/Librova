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
    public async Task BrowseLibraryRootAsync_AppliesPickedPath()
    {
        var viewModel = new FirstRunSetupViewModel(
            @"C:\Libraries\Librova",
            new FakePathSelectionService
            {
                SelectedWorkingDirectory = @"D:\Librova\Data"
            },
            new FakePreferencesStore(),
            _ => Task.CompletedTask);

        await viewModel.BrowseLibraryRootCommand.ExecuteAsyncForTests();

        Assert.Equal(@"D:\Librova\Data", viewModel.LibraryRoot);
        Assert.False(viewModel.HasValidationError);
    }

    [Fact]
    public async Task ContinueCommand_SavesPreferenceAfterSuccessfulCallback()
    {
        var preferences = new FakePreferencesStore();
        string? continuedWith = null;
        var viewModel = new FirstRunSetupViewModel(
            @"D:\Librova\Data",
            new FakePathSelectionService(),
            preferences,
            libraryRoot =>
            {
                continuedWith = libraryRoot;
                return Task.CompletedTask;
            });

        await viewModel.ContinueCommand.ExecuteAsyncForTests();

        Assert.Equal(@"D:\Librova\Data", preferences.LastSavedSnapshot?.PreferredLibraryRoot);
        Assert.Equal(@"D:\Librova\Data", continuedWith);
    }

    [Fact]
    public async Task ContinueCommand_DoesNotSavePreferenceWhenCallbackFails()
    {
        var preferences = new FakePreferencesStore();
        var viewModel = new FirstRunSetupViewModel(
            @"D:\Librova\Data",
            new FakePathSelectionService(),
            preferences,
            _ => throw new InvalidOperationException("startup failed"));

        await viewModel.ContinueCommand.ExecuteAsyncForTests();

        Assert.Null(preferences.LastSavedSnapshot);
        Assert.Contains("startup failed", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
    }

    private sealed class FakePathSelectionService : IPathSelectionService
    {
        public string? SelectedWorkingDirectory { get; init; }

        public Task<string?> PickSourceFileAsync(CancellationToken cancellationToken)
            => Task.FromResult<string?>(null);

        public Task<string?> PickWorkingDirectoryAsync(CancellationToken cancellationToken)
            => Task.FromResult(SelectedWorkingDirectory);

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
}
