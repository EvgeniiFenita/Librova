using Librova.UI.Runtime;

namespace Librova.UI.Shell;

internal static class FirstRunSetupPolicy
{
    public static bool RequiresSetup(IUiPreferencesStore? preferencesStore = null)
    {
        if (!string.IsNullOrWhiteSpace(RuntimeEnvironment.GetLibraryRootOverride()))
        {
            return false;
        }

        var snapshot = LoadSnapshot(preferencesStore);
        return !HasSavedLibraryPreference(snapshot);
    }

    public static string? BuildStartupRecoveryLibraryRootHint(
        IUiPreferencesStore? preferencesStore = null,
        string? baseDirectory = null,
        string? hostExecutableOverride = null)
    {
        if (!string.IsNullOrWhiteSpace(RuntimeEnvironment.GetLibraryRootOverride()))
        {
            return null;
        }

        var snapshot = LoadSnapshot(preferencesStore);
        if (!HasSavedLibraryPreference(snapshot))
        {
            return null;
        }

        var preferredLibraryRoot = RuntimeEnvironment.ResolvePreferredLibraryRoot(
            snapshot?.PreferredLibraryRoot,
            snapshot?.PortablePreferredLibraryRoot,
            baseDirectory,
            hostExecutableOverride);
        if (!string.IsNullOrWhiteSpace(preferredLibraryRoot))
        {
            return null;
        }

        if (!string.IsNullOrWhiteSpace(snapshot?.PreferredLibraryRoot))
        {
            return snapshot.PreferredLibraryRoot;
        }

        return string.IsNullOrWhiteSpace(snapshot?.PortablePreferredLibraryRoot)
            ? null
            : snapshot.PortablePreferredLibraryRoot;
    }

    private static UiPreferencesSnapshot? LoadSnapshot(IUiPreferencesStore? preferencesStore) =>
        (preferencesStore ?? UiPreferencesStore.CreateDefault()).TryLoad();

    private static bool HasSavedLibraryPreference(UiPreferencesSnapshot? snapshot) =>
        !string.IsNullOrWhiteSpace(snapshot?.PreferredLibraryRoot)
        || !string.IsNullOrWhiteSpace(snapshot?.PortablePreferredLibraryRoot);
}
