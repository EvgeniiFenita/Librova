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
        eventArgs.DragEffects = TryGetDroppedSourcePath(eventArgs) is null
            ? DragDropEffects.None
            : DragDropEffects.Copy;
        eventArgs.Handled = true;
    }

    private void OnDrop(object? sender, DragEventArgs eventArgs)
    {
        if (DataContext is not ShellWindowViewModel { HasShell: true, Shell: not null } viewModel)
        {
            return;
        }

        var sourcePath = TryGetDroppedSourcePath(eventArgs);
        viewModel.Shell.ImportJobs.ApplyDroppedSourcePath(sourcePath);
        eventArgs.Handled = true;
    }

    private static string? TryGetDroppedSourcePath(DragEventArgs eventArgs)
    {
        var storageItem = eventArgs.Data.GetFiles()?.FirstOrDefault();
        if (storageItem is null)
        {
            return null;
        }

        return GetLocalPath(storageItem);
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
