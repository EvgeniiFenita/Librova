using Avalonia.Controls;
using Avalonia.Input.Platform;
using Librova.UI.Logging;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.Desktop;

internal sealed class AvaloniaClipboardService : IClipboardService
{
    private readonly Window _ownerWindow;

    public AvaloniaClipboardService(Window ownerWindow)
    {
        _ownerWindow = ownerWindow;
    }

    public async Task<bool> SetTextAsync(string text, CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(text))
        {
            return false;
        }

        var topLevel = TopLevel.GetTopLevel(_ownerWindow);
        if (topLevel?.Clipboard is null)
        {
            UiLogging.Warning("Clipboard write requested, but no Avalonia clipboard is available.");
            return false;
        }

        cancellationToken.ThrowIfCancellationRequested();
        await topLevel.Clipboard.SetTextAsync(text);
        return true;
    }
}
