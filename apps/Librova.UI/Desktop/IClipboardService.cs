using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.Desktop;

internal interface IClipboardService
{
    Task<bool> SetTextAsync(string text, CancellationToken cancellationToken);
}
