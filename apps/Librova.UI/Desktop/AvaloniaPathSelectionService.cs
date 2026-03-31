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

    public async Task<IReadOnlyList<string>> PickSourceFilesAsync(CancellationToken cancellationToken)
    {
        var storageProvider = _ownerWindow.StorageProvider;
        if (storageProvider is null)
        {
            UiLogging.Warning("Source file selection requested, but no Avalonia storage provider is available.");
            return [];
        }

        var files = await storageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = "Select source books",
            AllowMultiple = true,
            FileTypeFilter =
            [
                new FilePickerFileType("Supported books")
                {
                    Patterns = ["*.fb2", "*.epub", "*.zip"]
                }
            ]
        });

        cancellationToken.ThrowIfCancellationRequested();

        var paths = files
            .Select(GetLocalPath)
            .Where(path => !string.IsNullOrWhiteSpace(path))
            .Cast<string>()
            .ToArray();
        UiLogging.Information("Source file selection completed. Count={Count}", paths.Length);
        return paths;
    }

    public async Task<string?> PickSourceDirectoryAsync(CancellationToken cancellationToken)
    {
        var storageProvider = _ownerWindow.StorageProvider;
        if (storageProvider is null)
        {
            UiLogging.Warning("Source directory selection requested, but no Avalonia storage provider is available.");
            return null;
        }

        var folders = await storageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions
        {
            Title = "Select source directory",
            AllowMultiple = false
        });

        cancellationToken.ThrowIfCancellationRequested();

        var path = folders.Count == 0 ? null : GetLocalPath(folders[0]);
        UiLogging.Information("Source directory selection completed. HasPath={HasPath}", !string.IsNullOrWhiteSpace(path));
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

    public async Task<string?> PickExportDestinationAsync(string suggestedFileName, CancellationToken cancellationToken)
    {
        var storageProvider = _ownerWindow.StorageProvider;
        if (storageProvider is null)
        {
            UiLogging.Warning("Export destination selection requested, but no Avalonia storage provider is available.");
            return null;
        }

        var file = await storageProvider.SaveFilePickerAsync(new FilePickerSaveOptions
        {
            Title = "Export selected book",
            SuggestedFileName = string.IsNullOrWhiteSpace(suggestedFileName) ? "book.epub" : suggestedFileName
        });

        cancellationToken.ThrowIfCancellationRequested();

        var path = file is null ? null : GetLocalPath(file);
        UiLogging.Information("Export destination selection completed. HasPath={HasPath}", !string.IsNullOrWhiteSpace(path));
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
