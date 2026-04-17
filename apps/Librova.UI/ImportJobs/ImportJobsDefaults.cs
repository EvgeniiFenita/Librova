using System.IO;
using Librova.UI.Runtime;

namespace Librova.UI.ImportJobs;

internal static class ImportJobsDefaults
{
    public static string BuildDefaultWorkingDirectory(string libraryRoot) =>
        RuntimeEnvironment.GetImportWorkspacePathForLibrary(libraryRoot);
}
