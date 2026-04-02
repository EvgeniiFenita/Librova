namespace Librova.UI.Views;

internal static class LibrarySelectionViewportCentering
{
    public static double CalculateCenteredVerticalOffset(
        double currentOffsetY,
        double viewportHeight,
        double extentHeight,
        double itemTopInViewport,
        double itemHeight)
    {
        var maximumOffset = extentHeight > viewportHeight
            ? extentHeight - viewportHeight
            : 0d;
        if (viewportHeight <= 0 || itemHeight <= 0 || maximumOffset <= 0)
        {
            return Clamp(currentOffsetY, 0d, maximumOffset);
        }

        var itemCenterInContent = currentOffsetY + itemTopInViewport + (itemHeight / 2d);
        var rawOffset = itemCenterInContent - (viewportHeight / 2d);
        return ClampVerticalOffset(rawOffset, viewportHeight, extentHeight);
    }

    public static double CalculateVisibleVerticalOffset(
        double currentOffsetY,
        double viewportHeight,
        double extentHeight,
        double itemTopInViewport,
        double itemHeight)
    {
        var maximumOffset = extentHeight > viewportHeight
            ? extentHeight - viewportHeight
            : 0d;
        if (viewportHeight <= 0 || itemHeight <= 0 || maximumOffset <= 0)
        {
            return Clamp(currentOffsetY, 0d, maximumOffset);
        }

        if (itemTopInViewport < 0d)
        {
            return ClampVerticalOffset(currentOffsetY + itemTopInViewport, viewportHeight, extentHeight);
        }

        var itemBottomInViewport = itemTopInViewport + itemHeight;
        if (itemBottomInViewport > viewportHeight)
        {
            return ClampVerticalOffset(currentOffsetY + (itemBottomInViewport - viewportHeight), viewportHeight, extentHeight);
        }

        return ClampVerticalOffset(currentOffsetY, viewportHeight, extentHeight);
    }

    public static double ClampVerticalOffset(double offsetY, double viewportHeight, double extentHeight)
    {
        var maximumOffset = extentHeight > viewportHeight
            ? extentHeight - viewportHeight
            : 0d;
        return Clamp(offsetY, 0d, maximumOffset);
    }

    private static double Clamp(double value, double minimum, double maximum)
    {
        if (value < minimum)
        {
            return minimum;
        }

        return value > maximum
            ? maximum
            : value;
    }
}
