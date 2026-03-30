using System.IO;

namespace Librova.UI.ImportJobs;

internal static class ImportJobsDefaults
{
    public static string BuildDefaultWorkingDirectory(string libraryRoot) =>
        Path.Combine(libraryRoot, "Temp", "UiImport");
}
