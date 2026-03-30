using System;
using System.IO;

namespace LibriFlow.UI.CoreHost;

internal static class CoreHostPathResolver
{
    public static string ResolveDevelopmentExecutablePath(string? baseDirectory = null)
    {
        var current = new DirectoryInfo(baseDirectory ?? AppContext.BaseDirectory);
        while (current is not null)
        {
            var candidate = Path.Combine(
                current.FullName,
                "out",
                "build",
                "x64-debug",
                "apps",
                "LibriFlow.Core.Host",
                "Debug",
                "LibriFlowCoreHostApp.exe");

            if (File.Exists(candidate))
            {
                return Path.GetFullPath(candidate);
            }

            current = current.Parent;
        }

        throw new FileNotFoundException(
            "Unable to resolve LibriFlowCoreHostApp.exe from the current repository layout.");
    }
}
