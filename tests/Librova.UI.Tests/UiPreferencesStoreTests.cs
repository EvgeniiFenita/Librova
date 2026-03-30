using Librova.UI.Shell;
using Xunit;

namespace Librova.UI.Tests;

public sealed class UiPreferencesStoreTests
{
    [Fact]
    public void SaveAndLoad_RoundTripsPreferredLibraryRoot()
    {
        var filePath = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}", "ui-preferences.json");
        var store = new UiPreferencesStore(filePath);
        var expected = new UiPreferencesSnapshot
        {
            PreferredLibraryRoot = @"D:\Librova\Library"
        };

        store.Save(expected);
        var actual = store.TryLoad();

        Assert.NotNull(actual);
        Assert.Equal(expected.PreferredLibraryRoot, actual!.PreferredLibraryRoot);
    }

    [Fact]
    public void Clear_RemovesPreferencesFile()
    {
        var filePath = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}", "ui-preferences.json");
        var store = new UiPreferencesStore(filePath);
        store.Save(new UiPreferencesSnapshot
        {
            PreferredLibraryRoot = @"D:\Librova\Library"
        });

        store.Clear();

        Assert.False(File.Exists(filePath));
    }
}
