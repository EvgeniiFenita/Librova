using Avalonia.Controls;
using Avalonia.Platform.Storage;
using Librova.UI.Logging;
using System;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.Desktop;

internal sealed class AvaloniaPathSelectionService : IPathSelectionService
{
    private readonly Window _ownerWindow;

    public AvaloniaPathSelectionService(Window ownerWindow)
    {
        _ownerWindow = ownerWindow;
    }

    public async Task<string?> PickSourceFileAsync(CancellationToken cancellationToken)
    {
        var storageProvider = _ownerWindow.StorageProvider;
        if (storageProvider is null)
        {
            UiLogging.Warning("Source file selection requested, but no Avalonia storage provider is available.");
            return null;
        }

        var files = await storageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = "Select source book",
            AllowMultiple = false,
            FileTypeFilter =
            [
                new FilePickerFileType("Supported books")
                {
                    Patterns = ["*.fb2", "*.epub", "*.zip"]
                }
            ]
        });

        cancellationToken.ThrowIfCancellationRequested();

        var path = files.Count == 0 ? null : GetLocalPath(files[0]);
        UiLogging.Information("Source file selection completed. HasPath={HasPath}", !string.IsNullOrWhiteSpace(path));
        return path;
    }

    public async Task<string?> PickWorkingDirectoryAsync(CancellationToken cancellationToken)
    {
        var storageProvider = _ownerWindow.StorageProvider;
        if (storageProvider is null)
        {
            UiLogging.Warning("Working directory selection requested, but no Avalonia storage provider is available.");
            return null;
        }

        var folders = await storageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions
        {
            Title = "Select working directory",
            AllowMultiple = false
        });

        cancellationToken.ThrowIfCancellationRequested();

        var path = folders.Count == 0 ? null : GetLocalPath(folders[0]);
        UiLogging.Information("Working directory selection completed. HasPath={HasPath}", !string.IsNullOrWhiteSpace(path));
        return path;
    }

    private static string? GetLocalPath(IStorageItem storageItem)
    {
        if (storageItem is null)
        {
            return null;
        }

        if (storageItem.TryGetLocalPath() is { Length: > 0 } localPath)
        {
            return localPath;
        }

        return storageItem.Path is null
            ? null
            : Uri.UnescapeDataString(storageItem.Path.LocalPath);
    }
}
