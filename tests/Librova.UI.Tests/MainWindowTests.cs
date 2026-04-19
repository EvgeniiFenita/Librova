using Avalonia.Input;
using Librova.UI.Views;
using Xunit;

namespace Librova.UI.Tests;

public sealed class MainWindowTests
{
    [Fact]
    public void NormalizeDroppedSourcePaths_FiltersEmptyEntries_AndDeduplicatesCaseInsensitively()
    {
        var result = MainWindow.NormalizeDroppedSourcePaths(
            [
                null,
                "",
                "   ",
                @"C:\Books\One.fb2",
                @"C:\Books\one.fb2",
                @"C:\Books\Two.epub"
            ]);

        Assert.Equal(
            [@"C:\Books\One.fb2", @"C:\Books\Two.epub"],
            result);
    }

    [Fact]
    public void GetDropEffect_ReturnsNone_ForEmptyResolvedSourceList()
    {
        var effect = MainWindow.GetDropEffect([]);

        Assert.Equal(DragDropEffects.None, effect);
    }

    [Fact]
    public void GetDropEffect_ReturnsCopy_ForAnyNonEmptySourceList()
    {
        var effect = MainWindow.GetDropEffect([@"C:\Books\One.fb2"]);

        Assert.Equal(DragDropEffects.Copy, effect);
    }
}
