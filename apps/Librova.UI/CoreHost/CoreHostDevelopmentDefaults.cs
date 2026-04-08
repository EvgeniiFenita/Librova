using System;
using System.IO;
using Librova.UI.Runtime;
using Librova.UI.Shell;

namespace Librova.UI.CoreHost;

internal static class CoreHostDevelopmentDefaults
{
    public static CoreHostLaunchOptions Create(string? baseDirectory = null, IUiPreferencesStore? preferencesStore = null)
    {
        var effectivePreferencesStore = preferencesStore ?? UiPreferencesStore.CreateDefault();
        var preferences = effectivePreferencesStore.TryLoad();
        var libraryRoot = RuntimeEnvironment.GetLibraryRootOverride();
        if (string.IsNullOrWhiteSpace(libraryRoot))
        {
            libraryRoot = RuntimeEnvironment.ResolvePreferredLibraryRoot(
                preferences?.PreferredLibraryRoot,
                preferences?.PortablePreferredLibraryRoot,
                baseDirectory,
                RuntimeEnvironment.GetCoreHostExecutableOverride());
        }

        return CreateForLibraryRoot(
            string.IsNullOrWhiteSpace(libraryRoot) ? GetFallbackLibraryRoot() : libraryRoot,
            preferences,
            baseDirectory,
            UiLibraryOpenMode.OpenExisting);
    }

    public static CoreHostLaunchOptions CreateForLibraryRoot(
        string libraryRoot,
        string? baseDirectory = null,
        UiLibraryOpenMode libraryOpenMode = UiLibraryOpenMode.OpenExisting)
    {
        var preferences = UiPreferencesStore.CreateDefault().TryLoad();
        return CreateForLibraryRoot(libraryRoot, preferences, baseDirectory, libraryOpenMode);
    }

    public static CoreHostLaunchOptions CreateForLibraryRoot(
        string libraryRoot,
        UiPreferencesSnapshot? preferences,
        string? baseDirectory = null,
        UiLibraryOpenMode libraryOpenMode = UiLibraryOpenMode.OpenExisting)
    {
        var executablePath = RuntimeEnvironment.GetCoreHostExecutableOverride()
            ?? CoreHostPathResolver.ResolveDevelopmentExecutablePath(baseDirectory);
        var pipePath = $@"\\.\pipe\Librova.UI.{Environment.ProcessId}.{Environment.TickCount64}";
        var hasConfiguredConverter = !string.IsNullOrWhiteSpace(preferences?.Fb2CngExecutablePath);

        return new CoreHostLaunchOptions
        {
            ExecutablePath = executablePath,
            PipePath = pipePath,
            LibraryRoot = libraryRoot,
            ShutdownEventName = $@"Local\Librova.UI.Shutdown.{Environment.ProcessId}.{Environment.TickCount64}",
            LibraryOpenMode = libraryOpenMode,
            ParentProcessId = Environment.ProcessId,
            ConverterMode = hasConfiguredConverter ? UiConverterMode.BuiltInFb2Cng : UiConverterMode.Disabled,
            Fb2CngExecutablePath = hasConfiguredConverter ? preferences!.Fb2CngExecutablePath : null
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
