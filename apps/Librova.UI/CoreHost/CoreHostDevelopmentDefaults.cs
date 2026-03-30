using System;
using System.IO;
using Librova.UI.Runtime;
using Librova.UI.Shell;

namespace Librova.UI.CoreHost;

internal static class CoreHostDevelopmentDefaults
{
    public static CoreHostLaunchOptions Create(string? baseDirectory = null, IUiPreferencesStore? preferencesStore = null)
    {
        var libraryRoot = RuntimeEnvironment.GetLibraryRootOverride();
        if (string.IsNullOrWhiteSpace(libraryRoot))
        {
            libraryRoot = (preferencesStore ?? UiPreferencesStore.CreateDefault()).TryLoad()?.PreferredLibraryRoot;
        }

        return CreateForLibraryRoot(
            string.IsNullOrWhiteSpace(libraryRoot) ? GetFallbackLibraryRoot() : libraryRoot,
            baseDirectory);
    }

    public static CoreHostLaunchOptions CreateForLibraryRoot(string libraryRoot, string? baseDirectory = null)
    {
        var preferences = UiPreferencesStore.CreateDefault().TryLoad();
        return CreateForLibraryRoot(libraryRoot, preferences, baseDirectory);
    }

    public static CoreHostLaunchOptions CreateForLibraryRoot(
        string libraryRoot,
        UiPreferencesSnapshot? preferences,
        string? baseDirectory = null)
    {
        var executablePath = RuntimeEnvironment.GetCoreHostExecutableOverride()
            ?? CoreHostPathResolver.ResolveDevelopmentExecutablePath(baseDirectory);
        var pipePath = $@"\\.\pipe\Librova.UI.{Environment.ProcessId}.{Environment.TickCount64}";

        return new CoreHostLaunchOptions
        {
            ExecutablePath = executablePath,
            PipePath = pipePath,
            LibraryRoot = libraryRoot,
            ConverterMode = preferences?.ConverterMode ?? UiConverterMode.Disabled,
            Fb2CngExecutablePath = preferences?.Fb2CngExecutablePath,
            Fb2CngConfigPath = preferences?.Fb2CngConfigPath,
            CustomConverterExecutablePath = preferences?.CustomConverterExecutablePath,
            CustomConverterArguments = preferences?.CustomConverterArguments ?? [],
            CustomConverterOutputMode = preferences?.CustomConverterOutputMode ?? UiConverterOutputMode.ExactDestinationPath
        };
    }

    public static string GetFallbackLibraryRoot()
    {
        var localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        if (string.IsNullOrWhiteSpace(localAppData))
        {
            throw new InvalidOperationException("LocalAppData is not available.");
        }

        return Path.Combine(localAppData, "Librova", "Library");
    }
}
