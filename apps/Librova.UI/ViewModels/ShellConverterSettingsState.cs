namespace Librova.UI.ViewModels;

internal sealed class ShellConverterSettingsState
{
    public string Fb2CngExecutablePath { get; set; } = string.Empty;
    public string ConverterValidationMessage { get; set; } = string.Empty;
    public bool IsConverterProbeInProgress { get; set; }

    public bool HasConverterValidationError =>
        !string.IsNullOrWhiteSpace(ConverterValidationMessage);

    public bool ShowConverterHelperText =>
        !HasConverterValidationError && !IsConverterProbeInProgress;

    public string ConverterProbeStatusText =>
        IsConverterProbeInProgress ? "Validating converter…" : string.Empty;

    public bool HasConfiguredConverter =>
        !string.IsNullOrWhiteSpace(Fb2CngExecutablePath);
}
