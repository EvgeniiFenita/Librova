using System;
using System.IO;
using Librova.UI.Logging;

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
        if (File.Exists(retainedLogPath))
        {
            using var source = new FileStream(runtimeLogPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
            using var destination = new FileStream(retainedLogPath, FileMode.Append, FileAccess.Write, FileShare.Read);
            if (destination.Length > 0)
            {
                using var separator = new StreamWriter(destination, leaveOpen: true);
                separator.WriteLine();
                separator.Flush();
            }

            source.CopyTo(destination);
        }
        else
        {
            File.Copy(runtimeLogPath, retainedLogPath, overwrite: false);
        }

        try
        {
            File.Delete(runtimeLogPath);
        }
        catch (IOException error)
        {
            LogRuntimeDeleteFailure(error, runtimeLogPath);
        }
        catch (UnauthorizedAccessException error)
        {
            LogRuntimeDeleteFailure(error, runtimeLogPath);
        }
    }

    private static void LogRuntimeDeleteFailure(Exception error, string runtimeLogPath)
    {
        if (UiLogging.CurrentLogFilePath is not null)
        {
            UiLogging.Warning(
                error,
                "Failed to delete runtime log after sync; old entries may persist. Path={Path}",
                runtimeLogPath);
            return;
        }

        Console.Error.WriteLine(
            $"Failed to delete runtime log after sync; old entries may persist. Path={runtimeLogPath}. {error.Message}");
    }
}
