using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.Desktop;

internal sealed class NullClipboardService : IClipboardService
{
    public Task<bool> SetTextAsync(string text, CancellationToken cancellationToken)
        => Task.FromResult(false);
}
