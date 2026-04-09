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

    public static string BuildOpenExistingValidationMessage(string libraryRoot, string? currentLibraryRoot = null)
    {
        if (string.IsNullOrWhiteSpace(libraryRoot) || !Directory.Exists(libraryRoot))
        {
            return "Open Library requires an existing Librova library root.";
        }

        if (IsNestedWithinCurrentLibraryRoot(libraryRoot, currentLibraryRoot))
        {
            return "Choose a folder outside the current library. Librova cannot open a nested subfolder as a separate library.";
        }

        return LooksLikeManagedLibraryRoot(libraryRoot)
            ? string.Empty
            : "Selected folder is not an existing Librova library.";
    }

    public static string BuildCreateNewValidationMessage(string libraryRoot, string? currentLibraryRoot = null)
    {
        if (string.IsNullOrWhiteSpace(libraryRoot) || !Directory.Exists(libraryRoot))
        {
            return string.Empty;
        }

        if (IsNestedWithinCurrentLibraryRoot(libraryRoot, currentLibraryRoot))
        {
            return "Choose a folder outside the current library. Librova cannot create a new library inside an existing managed library.";
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

    private static bool IsNestedWithinCurrentLibraryRoot(string selectedPath, string? currentLibraryRoot)
    {
        if (string.IsNullOrWhiteSpace(selectedPath)
            || string.IsNullOrWhiteSpace(currentLibraryRoot)
            || !Directory.Exists(currentLibraryRoot))
        {
            return false;
        }

        try
        {
            var normalizedSelectedPath = Path.GetFullPath(selectedPath);
            var normalizedCurrentLibraryRoot = Path.GetFullPath(currentLibraryRoot);
            if (string.Equals(normalizedSelectedPath, normalizedCurrentLibraryRoot, System.StringComparison.OrdinalIgnoreCase))
            {
                return false;
            }

            var relativePath = Path.GetRelativePath(normalizedCurrentLibraryRoot, normalizedSelectedPath);
            return !relativePath.Equals("..", System.StringComparison.Ordinal)
                && !relativePath.StartsWith($"..{Path.DirectorySeparatorChar}", System.StringComparison.Ordinal)
                && !relativePath.StartsWith($"..{Path.AltDirectorySeparatorChar}", System.StringComparison.Ordinal)
                && !Path.IsPathRooted(relativePath);
        }
        catch (IOException)
        {
            return false;
        }
        catch (UnauthorizedAccessException)
        {
            return false;
        }
    }
}
