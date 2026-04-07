using Librova.UI.CoreHost;

namespace Librova.UI.Shell;

internal static class UiPreferencesSnapshotBuilder
{
    public static UiPreferencesSnapshot WithPreferredLibraryRoot(
        UiPreferencesSnapshot? existing,
        string libraryRoot) =>
        BuildNormalizedSnapshot(existing, libraryRoot);

    private static UiPreferencesSnapshot BuildNormalizedSnapshot(
        UiPreferencesSnapshot? existing,
        string libraryRoot)
    {
        var executablePath = string.IsNullOrWhiteSpace(existing?.Fb2CngExecutablePath)
            ? null
            : existing.Fb2CngExecutablePath;

        return new UiPreferencesSnapshot
        {
            PreferredLibraryRoot = libraryRoot,
            ConverterMode = executablePath is null ? UiConverterMode.Disabled : UiConverterMode.BuiltInFb2Cng,
            Fb2CngExecutablePath = executablePath,
            Fb2CngConfigPath = executablePath is null ? null : existing?.Fb2CngConfigPath,
            ForceEpubConversionOnImport = executablePath is not null && (existing?.ForceEpubConversionOnImport ?? false),
            PreferredSortKey = existing?.PreferredSortKey,
            PreferredSortDescending = existing?.PreferredSortDescending ?? false
        };
    }
}
