using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Markup.Xaml;

namespace Librova.UI.Views;

internal sealed partial class ImportView : UserControl
{
    private int _dragOverCount;

    public ImportView()
    {
        AvaloniaXamlLoader.Load(this);

        var dropZone = this.FindControl<Border>("DropZoneBorder");
        if (dropZone is not null)
        {
            dropZone.AddHandler(DragDrop.DragEnterEvent, OnDropZoneDragEnter);
            dropZone.AddHandler(DragDrop.DragLeaveEvent, OnDropZoneDragLeave);
            dropZone.AddHandler(DragDrop.DropEvent, OnDropZoneDrop);
        }
    }

    private void OnDropZoneDragEnter(object? sender, DragEventArgs e)
    {
        if (sender is not Border border || !e.DataTransfer.Contains(DataFormat.File))
            return;

        if (++_dragOverCount == 1)
            border.Classes.Add("DragOver");
    }

    private void OnDropZoneDragLeave(object? sender, DragEventArgs e)
    {
        if (sender is not Border border)
            return;

        if (--_dragOverCount <= 0)
        {
            _dragOverCount = 0;
            border.Classes.Remove("DragOver");
        }
    }

    private void OnDropZoneDrop(object? sender, DragEventArgs e)
    {
        _dragOverCount = 0;
        if (sender is Border border)
            border.Classes.Remove("DragOver");
    }
}
