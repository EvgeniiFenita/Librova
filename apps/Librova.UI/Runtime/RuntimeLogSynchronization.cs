using System;
using System.IO;

namespace Librova.UI.Runtime;

internal static class RuntimeLogSynchronization
{
    public static void SyncPendingRuntimeLogs(string libraryRoot)
    {
        if (string.IsNullOrWhiteSpace(libraryRoot))
        {
            return;
        }

        SyncRuntimeLogFile(
            RuntimeEnvironment.GetUiRuntimeLogFilePathForLibrary(libraryRoot),
            RuntimeEnvironment.GetUiLogFilePathForLibrary(libraryRoot));
        SyncRuntimeLogFile(
            RuntimeEnvironment.GetHostRuntimeLogFilePathForLibrary(libraryRoot),
            RuntimeEnvironment.GetHostLogFilePathForLibrary(libraryRoot));
    }

    private static void SyncRuntimeLogFile(string runtimeLogPath, string retainedLogPath)
    {
        if (string.IsNullOrWhiteSpace(runtimeLogPath)
            || string.IsNullOrWhiteSpace(retainedLogPath)
            || !File.Exists(runtimeLogPath))
        {
            return;
        }

        Directory.CreateDirectory(Path.GetDirectoryName(retainedLogPath)!);
        File.Copy(runtimeLogPath, retainedLogPath, overwrite: true);

        try
        {
            File.Delete(runtimeLogPath);
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }
}
