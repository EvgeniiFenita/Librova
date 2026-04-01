using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.Desktop;

internal sealed class NullPathSelectionService : IPathSelectionService
{
    public Task<IReadOnlyList<string>> PickSourceFilesAsync(CancellationToken cancellationToken)
        => Task.FromResult<IReadOnlyList<string>>([]);

    public Task<string?> PickSourceDirectoryAsync(CancellationToken cancellationToken)
        => Task.FromResult<string?>(null);

    public Task<string?> PickWorkingDirectoryAsync(CancellationToken cancellationToken)
        => Task.FromResult<string?>(null);

    public Task<string?> PickExecutableFileAsync(string title, CancellationToken cancellationToken)
        => Task.FromResult<string?>(null);

    public Task<string?> PickExportDestinationAsync(string suggestedFileName, CancellationToken cancellationToken)
        => Task.FromResult<string?>(null);
}
