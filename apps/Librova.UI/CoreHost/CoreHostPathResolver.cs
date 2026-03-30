using System;
using System.IO;

namespace Librova.UI.CoreHost;

internal static class CoreHostPathResolver
{
    private static readonly (string Preset, string Configuration)[] KnownBuildConfigurations =
    [
        ("x64-debug", "Debug"),
        ("x64-release", "Release")
    ];

    public static string ResolveDevelopmentExecutablePath(string? baseDirectory = null)
        => ResolveDevelopmentExecutablePath(
            baseDirectory,
            Environment.GetEnvironmentVariable("LIBROVA_CORE_HOST_EXECUTABLE"));

    internal static string ResolveDevelopmentExecutablePath(string? baseDirectory, string? environmentOverride)
    {
        if (!string.IsNullOrWhiteSpace(environmentOverride))
        {
            var resolvedOverride = Path.GetFullPath(environmentOverride);
            if (File.Exists(resolvedOverride))
            {
                return resolvedOverride;
            }

            throw new FileNotFoundException(
                $"LIBROVA_CORE_HOST_EXECUTABLE points to a missing file: {resolvedOverride}");
        }

        var current = new DirectoryInfo(baseDirectory ?? AppContext.BaseDirectory);
        while (current is not null)
        {
            foreach (var (preset, configuration) in KnownBuildConfigurations)
            {
                var candidate = Path.Combine(
                    current.FullName,
                    "out",
                    "build",
                    preset,
                    "apps",
                    "Librova.Core.Host",
                    configuration,
                    "LibrovaCoreHostApp.exe");

                if (File.Exists(candidate))
                {
                    return Path.GetFullPath(candidate);
                }
            }

            current = current.Parent;
        }

        throw new FileNotFoundException(
            "Unable to resolve LibrovaCoreHostApp.exe from the current repository layout or environment override.");
    }
}
