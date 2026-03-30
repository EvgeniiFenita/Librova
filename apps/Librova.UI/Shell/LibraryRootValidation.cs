using System.IO;

namespace Librova.UI.Shell;

internal static class LibraryRootValidation
{
    public static string BuildValidationMessage(string libraryRoot)
    {
        if (string.IsNullOrWhiteSpace(libraryRoot))
        {
            return "Library root cannot be empty.";
        }

        if (!Path.IsPathFullyQualified(libraryRoot))
        {
            return "Use an absolute library root path.";
        }

        if (File.Exists(libraryRoot))
        {
            return "Library root must not point to a file.";
        }

        var rootPath = Path.GetPathRoot(libraryRoot);
        if (string.IsNullOrWhiteSpace(rootPath) || !Directory.Exists(rootPath))
        {
            return "Library root must be on an available drive.";
        }

        return string.Empty;
    }
}
