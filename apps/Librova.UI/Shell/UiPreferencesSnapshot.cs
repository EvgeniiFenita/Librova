namespace Librova.UI.Shell;

internal sealed class UiPreferencesSnapshot
{
    public string? PreferredLibraryRoot { get; init; }
    public CoreHost.UiConverterMode ConverterMode { get; init; }
    public string? Fb2CngExecutablePath { get; init; }
    public string? Fb2CngConfigPath { get; init; }
    public string? CustomConverterExecutablePath { get; init; }
    public string[]? CustomConverterArguments { get; init; }
    public CoreHost.UiConverterOutputMode CustomConverterOutputMode { get; init; }
    public bool ForceEpubConversionOnImport { get; init; }
}
