using System;
using System.IO;
using Librova.UI.Runtime;

namespace Librova.UI.CoreHost;

internal static class CoreHostDevelopmentDefaults
{
    public static CoreHostLaunchOptions Create(string? baseDirectory = null)
    {
        var libraryRoot = RuntimeEnvironment.GetLibraryRootOverride();
        if (string.IsNullOrWhiteSpace(libraryRoot))
        {
            var localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            if (string.IsNullOrWhiteSpace(localAppData))
            {
                throw new InvalidOperationException("LocalAppData is not available.");
            }

            libraryRoot = Path.Combine(localAppData, "Librova", "Library");
        }

        var executablePath = RuntimeEnvironment.GetCoreHostExecutableOverride()
            ?? CoreHostPathResolver.ResolveDevelopmentExecutablePath(baseDirectory);
        var pipePath = $@"\\.\pipe\Librova.UI.{Environment.ProcessId}.{Environment.TickCount64}";

        return new CoreHostLaunchOptions
        {
            ExecutablePath = executablePath,
            PipePath = pipePath,
            LibraryRoot = libraryRoot
        };
    }
}
