using Librova.UI.LibraryCatalog;

namespace Librova.UI.Shell;

internal sealed class UiPreferencesSnapshot
{
    public string? PreferredLibraryRoot { get; init; }
    public CoreHost.UiConverterMode ConverterMode { get; init; }
    public string? Fb2CngExecutablePath { get; init; }
    public string? Fb2CngConfigPath { get; init; }
    public bool ForceEpubConversionOnImport { get; init; }
    public BookSortModel? PreferredSortKey { get; init; }
    public bool PreferredSortDescending { get; init; }
}
