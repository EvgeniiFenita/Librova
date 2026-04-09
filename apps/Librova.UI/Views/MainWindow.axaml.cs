using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Markup.Xaml;
using Avalonia.Platform.Storage;
using Librova.UI.ViewModels;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;

namespace Librova.UI.Views;

internal sealed partial class MainWindow : Window
{
    // DWMWA_CAPTION_COLOR = 35, available on Windows 11 build 22000+
    private const uint DwmwaCaptionColor = 35;
    // #0D0A07 as COLORREF (0x00BBGGRR): R=0x0D G=0x0A B=0x07 → 0x00070A0D
    private const uint TitleBarColor = 0x00070A0D;

    [DllImport("dwmapi.dll")]
    private static extern int DwmSetWindowAttribute(IntPtr hwnd, uint dwAttribute, ref uint pvAttribute, uint cbAttribute);

    public MainWindow()
    {
        AvaloniaXamlLoader.Load(this);
        MinWidth = ShellLayoutMetrics.MinimumWindowWidth;
        MinHeight = ShellLayoutMetrics.MinimumWindowHeight;
        AddHandler(DragDrop.DragOverEvent, OnDragOver);
        AddHandler(DragDrop.DropEvent, OnDrop);
        Opened += OnOpened;
    }

    private static void OnOpened(object? sender, EventArgs e)
    {
        if (sender is not MainWindow window)
            return;

        try
        {
            var hwnd = window.TryGetPlatformHandle()?.Handle;
            if (hwnd is null or 0)
                return;

            var color = TitleBarColor;
            DwmSetWindowAttribute(hwnd.Value, DwmwaCaptionColor, ref color, sizeof(uint));
        }
        catch (Exception ex) when (ex is DllNotFoundException or EntryPointNotFoundException)
        {
            // dwmapi unavailable or DWMWA_CAPTION_COLOR not supported on this OS version
        }
    }

    private void OnDragOver(object? sender, DragEventArgs eventArgs)
    {
        if (DataContext is ShellWindowViewModel { HasShell: true, Shell.IsImportInProgress: true })
        {
            eventArgs.DragEffects = DragDropEffects.None;
            eventArgs.Handled = true;
            return;
        }

        eventArgs.DragEffects = eventArgs.DataTransfer?.Contains(DataFormat.File) == true
            ? DragDropEffects.Copy
            : DragDropEffects.None;
        eventArgs.Handled = true;
    }

    private async void OnDrop(object? sender, DragEventArgs eventArgs)
    {
        if (DataContext is not ShellWindowViewModel { HasShell: true, Shell: not null } viewModel)
        {
            return;
        }

        if (viewModel.Shell.IsImportInProgress)
        {
            eventArgs.Handled = true;
            return;
        }

        var sourcePaths = TryGetDroppedSourcePaths(eventArgs);
        await viewModel.Shell.ActivateImportSectionAsync();
        await viewModel.Shell.ImportJobs.ApplyDroppedSourcePathsAndStartAsync(sourcePaths);
        eventArgs.Handled = true;
    }

    private static IReadOnlyList<string> TryGetDroppedSourcePaths(DragEventArgs eventArgs)
    {
        var transfer = eventArgs.DataTransfer;
        if (transfer is null || !transfer.Contains(DataFormat.File))
        {
            return [];
        }

        var files = transfer.TryGetFiles();
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
