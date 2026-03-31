using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Markup.Xaml;
using Avalonia.Platform.Storage;
using Librova.UI.ViewModels;
using System;
using System.Linq;

namespace Librova.UI.Views;

internal sealed partial class MainWindow : Window
{
    public MainWindow()
    {
        AvaloniaXamlLoader.Load(this);
        AddHandler(DragDrop.DragOverEvent, OnDragOver);
        AddHandler(DragDrop.DropEvent, OnDrop);
    }

    private void OnDragOver(object? sender, DragEventArgs eventArgs)
    {
        eventArgs.DragEffects = TryGetDroppedSourcePaths(eventArgs).Count == 0
            ? DragDropEffects.None
            : DragDropEffects.Copy;
        eventArgs.Handled = true;
    }

    private async void OnDrop(object? sender, DragEventArgs eventArgs)
    {
        if (DataContext is not ShellWindowViewModel { HasShell: true, Shell: not null } viewModel)
        {
            return;
        }

        var sourcePaths = TryGetDroppedSourcePaths(eventArgs);
        await viewModel.Shell.ActivateImportSectionAsync();
        await viewModel.Shell.ImportJobs.ApplyDroppedSourcePathsAndStartAsync(sourcePaths);
        eventArgs.Handled = true;
    }

    private static IReadOnlyList<string> TryGetDroppedSourcePaths(DragEventArgs eventArgs)
    {
        var files = eventArgs.Data.GetFiles();
        if (files is null)
        {
            return [];
        }

        return files
            .Select(GetLocalPath)
            .Where(path => !string.IsNullOrWhiteSpace(path))
            .Cast<string>()
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToArray();
    }

    private static string? GetLocalPath(IStorageItem storageItem)
    {
        if (storageItem.TryGetLocalPath() is { Length: > 0 } localPath)
        {
            return localPath;
        }

        return storageItem.Path is null
            ? null
            : Uri.UnescapeDataString(storageItem.Path.LocalPath);
    }
}
