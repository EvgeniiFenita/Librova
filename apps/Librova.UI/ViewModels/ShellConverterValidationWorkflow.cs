using Librova.UI.Logging;
using Librova.UI.Shell;
using System;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class ShellConverterValidationWorkflow
{
    private readonly ConverterValidationCoordinator _converterValidationCoordinator;
    private readonly ShellConverterSettingsState _converterSettings;
    private readonly Func<Task> _savePreferencesAsync;
    private readonly Action<Task> _setProbeTask;
    private readonly Action<string> _setValidationMessage;
    private readonly Action _raiseProbeProperties;

    public ShellConverterValidationWorkflow(
        ConverterValidationCoordinator converterValidationCoordinator,
        ShellConverterSettingsState converterSettings,
        Func<Task> savePreferencesAsync,
        Action<Task> setProbeTask,
        Action<string> setValidationMessage,
        Action raiseProbeProperties)
    {
        _converterValidationCoordinator = converterValidationCoordinator;
        _converterSettings = converterSettings;
        _savePreferencesAsync = savePreferencesAsync;
        _setProbeTask = setProbeTask;
        _setValidationMessage = setValidationMessage;
        _raiseProbeProperties = raiseProbeProperties;
    }

    public bool CanSavePreferences() =>
        !_converterSettings.HasConverterValidationError && !_converterSettings.IsConverterProbeInProgress;

    public void ApplyState(bool allowAutoSave)
    {
        _setProbeTask(_converterValidationCoordinator.CurrentProbeTask);
        _converterSettings.IsConverterProbeInProgress = _converterValidationCoordinator.IsProbeInProgress;
        _setValidationMessage(_converterValidationCoordinator.ValidationMessage);
        _raiseProbeProperties();
        if (allowAutoSave && !_converterSettings.IsConverterProbeInProgress && !_converterSettings.HasConverterValidationError)
        {
            _ = AutoSavePreferencesAsync();
        }
    }

    public void Reset()
    {
        _setProbeTask(Task.CompletedTask);
        _converterSettings.IsConverterProbeInProgress = false;
        _setValidationMessage(string.Empty);
        _raiseProbeProperties();
    }

    private async Task AutoSavePreferencesAsync()
    {
        try
        {
            await _savePreferencesAsync();
        }
        catch (Exception ex)
        {
            UiLogging.Error(ex, "Failed to auto-save converter preferences.");
        }
    }
}
