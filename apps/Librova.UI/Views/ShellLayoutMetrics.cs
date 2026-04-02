namespace Librova.UI.Views;

internal static class ShellLayoutMetrics
{
    public const double WindowOuterMargin = 26;
    public const double WindowChromeHorizontalAllowance = 28;
    public const double WindowChromeVerticalAllowance = 48;
    public const double ShellNavigationRailWidth = 250;
    public const double ShellContentGap = 20;
    public const double LibraryToolbarRowGap = 18;
    public const double LibraryToolbarContentHeight = 44;
    public const double LibraryToolbarVerticalPadding = 40;
    public const double LibraryContentGap = 18;
    public const double LibraryGridCardPadding = 18;
    public const double LibraryCardWidth = 184;
    public const double LibraryCardHeight = 314;
    public const double LibraryCardHorizontalGap = 16;
    public const double LibraryCardVerticalGap = 16;
    public const double LibraryDetailsPanelWidth = 380;
    public const double LibraryViewportVerticalScrollbarAllowance = 20;
    public const double LibraryVisibleCardColumnCount = 2;
    public const double LibraryVisibleCardRowCount = 2;

    public const double LibraryToolbarFootprintHeight =
        LibraryToolbarContentHeight + LibraryToolbarVerticalPadding;

    // Keep the Library section usable on desktop: two visible cards must remain
    // available even when the details panel is open. Account for the book-grid
    // vertical scrollbar and native window chrome so the invariant still holds
    // in the real running app rather than only in ideal client-area math.
    public const double MinimumWindowWidth =
        (WindowOuterMargin * 2)
        + WindowChromeHorizontalAllowance
        + ShellNavigationRailWidth
        + ShellContentGap
        + (LibraryGridCardPadding * 2)
        + ((LibraryCardWidth + LibraryCardHorizontalGap) * LibraryVisibleCardColumnCount)
        + LibraryViewportVerticalScrollbarAllowance
        + LibraryContentGap
        + LibraryDetailsPanelWidth;

    public const double MinimumWindowHeight =
        (WindowOuterMargin * 2)
        + WindowChromeVerticalAllowance
        + LibraryToolbarFootprintHeight
        + LibraryToolbarRowGap
        + (LibraryGridCardPadding * 2)
        + ((LibraryCardHeight + LibraryCardVerticalGap) * LibraryVisibleCardRowCount);
}
