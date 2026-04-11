namespace Librova.UI.ViewModels;

internal sealed class ShellSectionState
{
    public ShellSection CurrentSection { get; set; } = ShellSection.Library;

    public bool IsLibrarySectionActive => CurrentSection is ShellSection.Library;
    public bool IsImportSectionActive => CurrentSection is ShellSection.Import;
    public bool IsSettingsSectionActive => CurrentSection is ShellSection.Settings;

    public string CurrentSectionTitle => CurrentSection switch
    {
        ShellSection.Library => "Library",
        ShellSection.Import => "Import",
        ShellSection.Settings => "Settings",
        _ => "Librova"
    };

    public string CurrentSectionDescription => CurrentSection switch
    {
        ShellSection.Library => "Browse the managed library as a visual grid, inspect book metadata, export books, and move books to Recycle Bin.",
        ShellSection.Import => "Bring EPUB, FB2, and ZIP sources into the managed library through the native import pipeline.",
        ShellSection.Settings => "Adjust converter preferences and inspect runtime diagnostics.",
        _ => string.Empty
    };
}
