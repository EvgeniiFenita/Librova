using System;
using System.IO;
using System.Linq;
using Librova.UI.Logging;

namespace Librova.UI.Runtime;

internal static class RuntimeWorkspaceMaintenance
{
    public static void PrepareForSession(string libraryRoot)
    {
        if (string.IsNullOrWhiteSpace(libraryRoot))
        {
            return;
        }

        var runtimeWorkspaceRoot = RuntimeEnvironment.GetRuntimeWorkspaceRootForLibrary(
            libraryRoot,
            AppContext.BaseDirectory,
            RuntimeEnvironment.GetCoreHostExecutableOverride());

        CleanupOwnedDirectory(Path.Combine(runtimeWorkspaceRoot, "ImportWorkspaces", "GeneratedUiImportWorkspace"));
        CleanupEmptyParents(Path.Combine(runtimeWorkspaceRoot, "ImportWorkspaces"), runtimeWorkspaceRoot);
        CleanupOwnedDirectory(Path.Combine(runtimeWorkspaceRoot, "ConverterWorkspace"));
        CleanupOwnedDirectory(Path.Combine(runtimeWorkspaceRoot, "ManagedStorageStaging"));
        CleanupOwnedDirectory(Path.Combine(runtimeWorkspaceRoot, "RuntimeLogs"));
        CleanupEmptyParents(runtimeWorkspaceRoot, runtimeWorkspaceRoot);
    }

    internal static void PrepareForSession(string libraryRoot, string baseDirectory, string? hostExecutableOverride)
    {
        if (string.IsNullOrWhiteSpace(libraryRoot))
        {
            return;
        }

        var runtimeWorkspaceRoot = RuntimeEnvironment.GetRuntimeWorkspaceRootForLibrary(
            libraryRoot,
            baseDirectory,
            hostExecutableOverride);

        CleanupOwnedDirectory(Path.Combine(runtimeWorkspaceRoot, "ImportWorkspaces", "GeneratedUiImportWorkspace"));
        CleanupEmptyParents(Path.Combine(runtimeWorkspaceRoot, "ImportWorkspaces"), runtimeWorkspaceRoot);
        CleanupOwnedDirectory(Path.Combine(runtimeWorkspaceRoot, "ConverterWorkspace"));
        CleanupOwnedDirectory(Path.Combine(runtimeWorkspaceRoot, "ManagedStorageStaging"));
        CleanupOwnedDirectory(Path.Combine(runtimeWorkspaceRoot, "RuntimeLogs"));
        CleanupEmptyParents(runtimeWorkspaceRoot, runtimeWorkspaceRoot);
    }

    private static void CleanupOwnedDirectory(string path)
    {
        if (string.IsNullOrWhiteSpace(path) || !Directory.Exists(path))
        {
            return;
        }

        try
        {
            Directory.Delete(path, recursive: true);
        }
        catch (IOException error)
        {
            UiLogging.Warning(error, "Failed to clean up owned runtime workspace directory. Path={Path}", path);
        }
        catch (UnauthorizedAccessException error)
        {
            UiLogging.Warning(error, "Failed to clean up owned runtime workspace directory. Path={Path}", path);
        }
    }

    private static void CleanupEmptyParents(string startPath, string stopPathInclusive)
    {
        if (string.IsNullOrWhiteSpace(startPath) || string.IsNullOrWhiteSpace(stopPathInclusive))
        {
            return;
        }

        var current = Path.GetFullPath(startPath);
        var stop = Path.GetFullPath(stopPathInclusive);

        while (IsSameOrDescendantPath(current, stop))
        {
            if (!Directory.Exists(current))
            {
                if (string.Equals(current, stop, StringComparison.OrdinalIgnoreCase))
                {
                    break;
                }

                current = Path.GetDirectoryName(current) ?? string.Empty;
                continue;
            }

            try
            {
                if (Directory.EnumerateFileSystemEntries(current).Any())
                {
                    break;
                }

                Directory.Delete(current, recursive: false);
            }
            catch (IOException)
            {
                break;
            }
            catch (UnauthorizedAccessException)
            {
                break;
            }

            if (string.Equals(current, stop, StringComparison.OrdinalIgnoreCase))
            {
                break;
            }

            current = Path.GetDirectoryName(current) ?? string.Empty;
        }
    }

    private static bool IsSameOrDescendantPath(string candidatePath, string rootPath)
    {
        var normalizedCandidate = EnsureTrailingSeparator(Path.GetFullPath(candidatePath));
        var normalizedRoot = EnsureTrailingSeparator(Path.GetFullPath(rootPath));
        return normalizedCandidate.StartsWith(normalizedRoot, StringComparison.OrdinalIgnoreCase);
    }

    private static string EnsureTrailingSeparator(string path) =>
        path.EndsWith(Path.DirectorySeparatorChar)
            || path.EndsWith(Path.AltDirectorySeparatorChar)
                ? path
                : path + Path.DirectorySeparatorChar;
}
