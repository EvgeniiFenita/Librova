using Librova.UI.CoreHost;
using Librova.UI.Desktop;
using Librova.UI.Logging;
using Librova.UI.Shell;
using System;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class ShellLibrarySwitchController
{
    private readonly IPathSelectionService _pathSelectionService;
    private readonly string _currentLibraryRoot;
    private readonly Func<string, UiLibraryOpenMode, Task>? _switchLibraryAsync;

    public ShellLibrarySwitchController(
        IPathSelectionService pathSelectionService,
        string currentLibraryRoot,
        Func<string, UiLibraryOpenMode, Task>? switchLibraryAsync)
    {
        _pathSelectionService = pathSelectionService;
        _currentLibraryRoot = currentLibraryRoot;
        _switchLibraryAsync = switchLibraryAsync;
    }

    public async Task OpenLibraryAsync()
    {
        var selectedPath = await _pathSelectionService.PickWorkingDirectoryAsync(default);
        if (string.IsNullOrWhiteSpace(selectedPath) || _switchLibraryAsync is null)
        {
            return;
        }

        var validationMessage = LibraryRootInspection.BuildOpenExistingValidationMessage(selectedPath, _currentLibraryRoot);
        if (!string.IsNullOrWhiteSpace(validationMessage))
        {
            UiLogging.Warning(
                "Rejected Open Library request. LibraryRoot={LibraryRoot} Validation={Validation}",
                selectedPath,
                validationMessage);
            throw new InvalidOperationException(validationMessage);
        }

        await _switchLibraryAsync(selectedPath, UiLibraryOpenMode.OpenExisting);
    }

    public async Task CreateLibraryAsync()
    {
        var selectedPath = await _pathSelectionService.PickWorkingDirectoryAsync(default);
        if (string.IsNullOrWhiteSpace(selectedPath) || _switchLibraryAsync is null)
        {
            return;
        }

        var validationMessage = LibraryRootInspection.BuildCreateNewValidationMessage(selectedPath, _currentLibraryRoot);
        if (!string.IsNullOrWhiteSpace(validationMessage))
        {
            UiLogging.Warning(
                "Rejected Create Library request. LibraryRoot={LibraryRoot} Validation={Validation}",
                selectedPath,
                validationMessage);
            throw new InvalidOperationException(validationMessage);
        }

        await _switchLibraryAsync(selectedPath, UiLibraryOpenMode.CreateNew);
    }
}
