using Librova.UI.Shell;
using Xunit;

namespace Librova.UI.Tests;

public sealed class ShellStateStoreTests
{
    [Fact]
    public void SaveAndLoad_RoundTripsShellStateSnapshot()
    {
        var filePath = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}", "ui-shell-state.json");
        var store = new ShellStateStore(filePath);
        var expected = new ShellStateSnapshot
        {
            SourcePaths = [@"C:\Incoming\saved.fb2", @"C:\Incoming\saved.epub"],
            WorkingDirectory = @"C:\Temp\Librova\UiImport",
            AllowProbableDuplicates = true
        };

        store.Save(expected);
        var actual = store.TryLoad();

        Assert.NotNull(actual);
        Assert.Equal(expected.SourcePaths, actual!.SourcePaths);
        Assert.Equal(expected.WorkingDirectory, actual.WorkingDirectory);
        Assert.Equal(expected.AllowProbableDuplicates, actual.AllowProbableDuplicates);
    }

    [Fact]
    public void TryLoad_ReturnsNullForMissingStateFile()
    {
        var filePath = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}", "ui-shell-state.json");
        var store = new ShellStateStore(filePath);

        Assert.Null(store.TryLoad());
    }
}
