using Librova.UI.Desktop;
using System;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class ShellConverterPathController
{
    private readonly IPathSelectionService _pathSelectionService;
    private readonly Action<string> _setExecutablePath;

    public ShellConverterPathController(
        IPathSelectionService pathSelectionService,
        Action<string> setExecutablePath)
    {
        _pathSelectionService = pathSelectionService;
        _setExecutablePath = setExecutablePath;
    }

    public Task ClearConverterPathAsync()
    {
        _setExecutablePath(string.Empty);
        return Task.CompletedTask;
    }

    public async Task BrowseFb2CngExecutablePathAsync()
    {
        var selectedPath = await _pathSelectionService.PickExecutableFileAsync("Select fb2cng executable", default);
        if (!string.IsNullOrWhiteSpace(selectedPath))
        {
            _setExecutablePath(selectedPath);
        }
    }
}
