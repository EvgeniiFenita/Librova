namespace Librova.UI.ViewModels;

internal enum ShellNavigationKind
{
    Library,
    Collection,
    Import,
    Settings
}

internal sealed record ShellNavigationSelection(ShellNavigationKind Kind, long? CollectionId = null)
{
    public static ShellNavigationSelection Library { get; } = new(ShellNavigationKind.Library);
    public static ShellNavigationSelection Import { get; } = new(ShellNavigationKind.Import);
    public static ShellNavigationSelection Settings { get; } = new(ShellNavigationKind.Settings);

    public static ShellNavigationSelection Collection(long collectionId) =>
        new(ShellNavigationKind.Collection, collectionId);
}
