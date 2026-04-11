using Librova.UI.CoreHost;
using Librova.UI.LibraryCatalog;
using Librova.UI.Logging;
using Librova.UI.Runtime;
using Librova.UI.Shell;
using System;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class ShellPreferencesPersistence
{
    private readonly IUiPreferencesStore _preferencesStore;
    private readonly string _libraryRoot;
    private readonly Func<Task>? _reloadShellAsync;

    public ShellPreferencesPersistence(
        IUiPreferencesStore preferencesStore,
        string libraryRoot,
        Func<Task>? reloadShellAsync = null)
    {
        _preferencesStore = preferencesStore;
        _libraryRoot = libraryRoot;
        _reloadShellAsync = reloadShellAsync;
    }

    public Task SaveConverterPreferencesAsync(
        string executablePath,
        bool forceEpubConversionOnImport,
        BookSortModel? preferredSortKey,
        bool preferredSortDescending)
    {
        var hasConfiguredConverter = !string.IsNullOrWhiteSpace(executablePath);
        var current = _preferencesStore.TryLoad();
        _preferencesStore.Save(BuildPreferencesSnapshot(new UiPreferencesSnapshot
        {
            PreferredLibraryRoot = _libraryRoot,
            ConverterMode = hasConfiguredConverter ? UiConverterMode.BuiltInFb2Cng : UiConverterMode.Disabled,
            Fb2CngExecutablePath = hasConfiguredConverter ? executablePath : null,
            Fb2CngConfigPath = hasConfiguredConverter ? current?.Fb2CngConfigPath : null,
            ForceEpubConversionOnImport = hasConfiguredConverter && forceEpubConversionOnImport,
            PreferredSortKey = preferredSortKey,
            PreferredSortDescending = preferredSortDescending
        }));
        UiLogging.Information("Saved converter preferences for current library. LibraryRoot={LibraryRoot}", _libraryRoot);
        return _reloadShellAsync is null ? Task.CompletedTask : _reloadShellAsync();
    }

    public void TrySaveSortPreference(BookSortModel sortKey, bool sortDescending)
    {
        try
        {
            var current = _preferencesStore.TryLoad();
            _preferencesStore.Save(BuildPreferencesSnapshot(new UiPreferencesSnapshot
            {
                PreferredLibraryRoot = _libraryRoot,
                ConverterMode = current?.ConverterMode ?? UiConverterMode.Disabled,
                PortablePreferredLibraryRoot = current?.PortablePreferredLibraryRoot,
                Fb2CngExecutablePath = current?.Fb2CngExecutablePath,
                Fb2CngConfigPath = current?.Fb2CngConfigPath,
                ForceEpubConversionOnImport = current?.ForceEpubConversionOnImport ?? false,
                PreferredSortKey = sortKey,
                PreferredSortDescending = sortDescending
            }));
        }
        catch (Exception error)
        {
            UiLogging.Warning(error, "Failed to persist sort preference.");
        }
    }

    private UiPreferencesSnapshot BuildPreferencesSnapshot(UiPreferencesSnapshot snapshot)
    {
        var libraryRoot = snapshot.PreferredLibraryRoot ?? _libraryRoot;
        return new UiPreferencesSnapshot
        {
            PreferredLibraryRoot = libraryRoot,
            PortablePreferredLibraryRoot = RuntimeEnvironment.BuildPortableLibraryRootPreference(libraryRoot),
            ConverterMode = snapshot.ConverterMode,
            Fb2CngExecutablePath = snapshot.Fb2CngExecutablePath,
            Fb2CngConfigPath = snapshot.Fb2CngConfigPath,
            ForceEpubConversionOnImport = snapshot.ForceEpubConversionOnImport,
            PreferredSortKey = snapshot.PreferredSortKey,
            PreferredSortDescending = snapshot.PreferredSortDescending
        };
    }
}
