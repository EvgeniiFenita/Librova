using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.Desktop;

internal sealed class NullPathSelectionService : IPathSelectionService
{
    public Task<string?> PickSourceFileAsync(CancellationToken cancellationToken)
        => Task.FromResult<string?>(null);

    public Task<string?> PickWorkingDirectoryAsync(CancellationToken cancellationToken)
        => Task.FromResult<string?>(null);

    public Task<string?> PickExportDestinationAsync(string suggestedFileName, CancellationToken cancellationToken)
        => Task.FromResult<string?>(null);
}
