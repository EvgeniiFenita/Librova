using Librova.UI.CoreHost;
using System.IO;
using System.Linq;

namespace Librova.UI.Shell;

internal static class LibraryRootInspection
{
    private static readonly string[] RequiredManagedDirectories =
    [
        "Database",
        "Books",
        "Covers",
        "Logs",
        "Temp",
        "Trash"
    ];

    public static string BuildModeValidationMessage(string libraryRoot, UiLibraryOpenMode mode)
    {
        return mode switch
        {
            UiLibraryOpenMode.OpenExisting => BuildOpenExistingValidationMessage(libraryRoot),
            UiLibraryOpenMode.CreateNew => BuildCreateNewValidationMessage(libraryRoot),
            _ => string.Empty
        };
    }

    public static string BuildOpenExistingValidationMessage(string libraryRoot)
    {
        if (string.IsNullOrWhiteSpace(libraryRoot) || !Directory.Exists(libraryRoot))
        {
            return "Open Library requires an existing Librova library root.";
        }

        return LooksLikeManagedLibraryRoot(libraryRoot)
            ? string.Empty
            : "Selected folder is not an existing Librova library.";
    }

    public static string BuildCreateNewValidationMessage(string libraryRoot)
    {
        if (string.IsNullOrWhiteSpace(libraryRoot) || !Directory.Exists(libraryRoot))
        {
            return string.Empty;
        }

        try
        {
            return Directory.EnumerateFileSystemEntries(libraryRoot).Any()
                ? "Create Library requires an empty target directory."
                : string.Empty;
        }
        catch (IOException)
        {
            return "Create Library requires a writable target directory.";
        }
        catch (UnauthorizedAccessException)
        {
            return "Create Library requires a writable target directory.";
        }
    }

    public static UiLibraryOpenMode ResolveModeForRecovery(string libraryRoot) =>
        LooksLikeManagedLibraryRoot(libraryRoot)
            ? UiLibraryOpenMode.OpenExisting
            : UiLibraryOpenMode.CreateNew;

    public static bool LooksLikeManagedLibraryRoot(string libraryRoot)
    {
        if (string.IsNullOrWhiteSpace(libraryRoot) || !Directory.Exists(libraryRoot))
        {
            return false;
        }

        return RequiredManagedDirectories.All(directoryName =>
                   Directory.Exists(Path.Combine(libraryRoot, directoryName)))
            && File.Exists(Path.Combine(libraryRoot, "Database", "librova.db"));
    }
}
