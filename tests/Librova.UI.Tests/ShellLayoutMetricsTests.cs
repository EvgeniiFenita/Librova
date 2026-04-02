using Librova.UI.Views;
using Xunit;

namespace Librova.UI.Tests;

public sealed class ShellLayoutMetricsTests
{
    [Fact]
    public void MinimumWindowWidth_PreservesTwoLibraryCardColumnsWithDetailsPanelOpen()
    {
        var requiredWidth =
            (ShellLayoutMetrics.WindowOuterMargin * 2)
            + ShellLayoutMetrics.WindowChromeHorizontalAllowance
            + ShellLayoutMetrics.ShellNavigationRailWidth
            + ShellLayoutMetrics.ShellContentGap
            + (ShellLayoutMetrics.LibraryGridCardPadding * 2)
            + ((ShellLayoutMetrics.LibraryCardWidth + ShellLayoutMetrics.LibraryCardHorizontalGap)
                * ShellLayoutMetrics.LibraryVisibleCardColumnCount)
            + ShellLayoutMetrics.LibraryViewportVerticalScrollbarAllowance
            + ShellLayoutMetrics.LibraryContentGap
            + ShellLayoutMetrics.LibraryDetailsPanelWidth;

        Assert.Equal(ShellLayoutMetrics.MinimumWindowWidth, requiredWidth);
        Assert.True(ShellLayoutMetrics.MinimumWindowWidth > 1156);
    }

    [Fact]
    public void MinimumWindowHeight_PreservesTwoLibraryCardRows()
    {
        var requiredHeight =
            (ShellLayoutMetrics.WindowOuterMargin * 2)
            + ShellLayoutMetrics.WindowChromeVerticalAllowance
            + ShellLayoutMetrics.LibraryToolbarFootprintHeight
            + ShellLayoutMetrics.LibraryToolbarRowGap
            + (ShellLayoutMetrics.LibraryGridCardPadding * 2)
            + ((ShellLayoutMetrics.LibraryCardHeight + ShellLayoutMetrics.LibraryCardVerticalGap)
                * ShellLayoutMetrics.LibraryVisibleCardRowCount)
            + 140;

        Assert.Equal(ShellLayoutMetrics.MinimumWindowHeight, requiredHeight);
        Assert.True(ShellLayoutMetrics.MinimumWindowHeight > 960);
    }
}
