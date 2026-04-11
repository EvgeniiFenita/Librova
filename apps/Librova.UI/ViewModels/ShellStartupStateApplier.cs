using Librova.UI.CoreHost;
using Librova.UI.ImportJobs;
using Librova.UI.Shell;
using System;

namespace Librova.UI.ViewModels;

internal sealed class ShellStartupStateApplier
{
    private readonly CoreHostLaunchOptions _hostOptions;
    private readonly ShellLaunchOptions? _launchOptions;
    private readonly ShellStateSnapshot? _savedState;
    private readonly UiPreferencesSnapshot? _savedPreferences;
    private readonly ImportJobsViewModel _importJobs;
    private readonly ShellConverterSettingsState _converterSettings;
    private readonly Action<ShellSection> _setCurrentSection;

    public ShellStartupStateApplier(
        CoreHostLaunchOptions hostOptions,
        ShellLaunchOptions? launchOptions,
        ShellStateSnapshot? savedState,
        UiPreferencesSnapshot? savedPreferences,
        ImportJobsViewModel importJobs,
        ShellConverterSettingsState converterSettings,
        Action<ShellSection> setCurrentSection)
    {
        _hostOptions = hostOptions;
        _launchOptions = launchOptions;
        _savedState = savedState;
        _savedPreferences = savedPreferences;
        _importJobs = importJobs;
        _converterSettings = converterSettings;
        _setCurrentSection = setCurrentSection;
    }

    public bool Apply()
    {
        var hasConfiguredConverter = HasConfiguredBuiltInConverter(_hostOptions);

        _importJobs.WorkingDirectory = string.IsNullOrWhiteSpace(_savedState?.WorkingDirectory)
            ? ImportJobsDefaults.BuildDefaultWorkingDirectory(_hostOptions.LibraryRoot)
            : _savedState.WorkingDirectory!;
        _importJobs.AllowProbableDuplicates = _savedState?.AllowProbableDuplicates ?? false;
        _importJobs.HasConfiguredConverter = hasConfiguredConverter;
        _importJobs.ForceEpubConversionOnImport = hasConfiguredConverter
            && (_savedPreferences?.ForceEpubConversionOnImport ?? false);
        _converterSettings.Fb2CngExecutablePath = _savedPreferences?.Fb2CngExecutablePath ?? string.Empty;

        if (_hostOptions.LibraryOpenMode == UiLibraryOpenMode.CreateNew)
        {
            _setCurrentSection(ShellSection.Import);
        }

        if (_launchOptions?.InitialSourcePaths is { Length: > 0 } launchSourcePaths)
        {
            _importJobs.ApplyDroppedSourcePaths(launchSourcePaths);
        }
        else if (_savedState?.SourcePaths is { Length: > 0 } savedSourcePaths)
        {
            _importJobs.ApplyDroppedSourcePaths(savedSourcePaths);
        }

        return hasConfiguredConverter;
    }

    private static bool HasConfiguredBuiltInConverter(CoreHostLaunchOptions hostOptions) =>
        hostOptions.ConverterMode is UiConverterMode.BuiltInFb2Cng
        && !string.IsNullOrWhiteSpace(hostOptions.Fb2CngExecutablePath);
}
