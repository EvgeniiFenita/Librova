using System;
using System.IO;

namespace Librova.UI.CoreHost;

internal static class CoreHostDevelopmentDefaults
{
    public static CoreHostLaunchOptions Create(string? baseDirectory = null)
    {
        var localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        if (string.IsNullOrWhiteSpace(localAppData))
        {
            throw new InvalidOperationException("LocalAppData is not available.");
        }

        var libraryRoot = Path.Combine(localAppData, "Librova", "Library");
        var executablePath = CoreHostPathResolver.ResolveDevelopmentExecutablePath(baseDirectory);
        var pipePath = $@"\\.\pipe\Librova.UI.{Environment.ProcessId}.{Environment.TickCount64}";

        return new CoreHostLaunchOptions
        {
            ExecutablePath = executablePath,
            PipePath = pipePath,
            LibraryRoot = libraryRoot
        };
    }
}
