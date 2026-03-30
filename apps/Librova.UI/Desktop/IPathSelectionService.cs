using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.Desktop;

internal interface IPathSelectionService
{
    Task<string?> PickSourceFileAsync(CancellationToken cancellationToken);
    Task<string?> PickWorkingDirectoryAsync(CancellationToken cancellationToken);
}
