using Librova.UI.CoreHost;

namespace Librova.UI.Shell;

internal static class UiPreferencesSnapshotBuilder
{
    public static UiPreferencesSnapshot WithPreferredLibraryRoot(
        UiPreferencesSnapshot? existing,
        string libraryRoot) =>
        new()
        {
            PreferredLibraryRoot = libraryRoot,
            ConverterMode = existing?.ConverterMode ?? UiConverterMode.Disabled,
            Fb2CngExecutablePath = existing?.Fb2CngExecutablePath,
            Fb2CngConfigPath = existing?.Fb2CngConfigPath,
            CustomConverterExecutablePath = existing?.CustomConverterExecutablePath,
            CustomConverterArguments = existing?.CustomConverterArguments,
            CustomConverterOutputMode = existing?.CustomConverterOutputMode ?? UiConverterOutputMode.ExactDestinationPath,
            ForceEpubConversionOnImport = existing?.ForceEpubConversionOnImport ?? false
        };
}
