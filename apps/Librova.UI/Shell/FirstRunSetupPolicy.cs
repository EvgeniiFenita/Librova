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

        var snapshot = (preferencesStore ?? UiPreferencesStore.CreateDefault()).TryLoad();
        var preferredLibraryRoot = RuntimeEnvironment.ResolvePreferredLibraryRoot(
            snapshot?.PreferredLibraryRoot,
            snapshot?.PortablePreferredLibraryRoot);
        return string.IsNullOrWhiteSpace(preferredLibraryRoot);
    }
}
