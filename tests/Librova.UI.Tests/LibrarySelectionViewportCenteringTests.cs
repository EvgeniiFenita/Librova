using Librova.UI.Views;
using Xunit;

namespace Librova.UI.Tests;

public sealed class LibrarySelectionViewportCenteringTests
{
    [Fact]
    public void CalculateCenteredVerticalOffset_CentersTheCardWithinTheCurrentViewport()
    {
        var offset = LibrarySelectionViewportCentering.CalculateCenteredVerticalOffset(
            currentOffsetY: 640,
            viewportHeight: 400,
            extentHeight: 1800,
            itemTopInViewport: 280,
            itemHeight: 314);

        Assert.Equal(877, offset, precision: 6);
    }

    [Fact]
    public void CalculateCenteredVerticalOffset_ClampsToTheTopOfTheScrollableExtent()
    {
        var offset = LibrarySelectionViewportCentering.CalculateCenteredVerticalOffset(
            currentOffsetY: 0,
            viewportHeight: 500,
            extentHeight: 1800,
            itemTopInViewport: 20,
            itemHeight: 314);

        Assert.Equal(0, offset, precision: 6);
    }

    [Fact]
    public void CalculateCenteredVerticalOffset_ClampsToTheBottomOfTheScrollableExtent()
    {
        var offset = LibrarySelectionViewportCentering.CalculateCenteredVerticalOffset(
            currentOffsetY: 700,
            viewportHeight: 400,
            extentHeight: 1000,
            itemTopInViewport: 300,
            itemHeight: 314);

        Assert.Equal(600, offset, precision: 6);
    }

    [Fact]
    public void CalculateVisibleVerticalOffset_LeavesAlreadyVisibleCardUntouched()
    {
        var offset = LibrarySelectionViewportCentering.CalculateVisibleVerticalOffset(
            currentOffsetY: 320,
            viewportHeight: 400,
            extentHeight: 1800,
            itemTopInViewport: 40,
            itemHeight: 314);

        Assert.Equal(320, offset, precision: 6);
    }

    [Fact]
    public void CalculateVisibleVerticalOffset_ScrollsJustEnoughToRevealCardBottom()
    {
        var offset = LibrarySelectionViewportCentering.CalculateVisibleVerticalOffset(
            currentOffsetY: 320,
            viewportHeight: 400,
            extentHeight: 1800,
            itemTopInViewport: 180,
            itemHeight: 314);

        Assert.Equal(414, offset, precision: 6);
    }
}

