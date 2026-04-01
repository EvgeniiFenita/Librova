using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.Desktop;

internal interface IPathSelectionService
{
    Task<IReadOnlyList<string>> PickSourceFilesAsync(CancellationToken cancellationToken);
    Task<string?> PickSourceDirectoryAsync(CancellationToken cancellationToken);
    Task<string?> PickWorkingDirectoryAsync(CancellationToken cancellationToken);
    Task<string?> PickExecutableFileAsync(string title, CancellationToken cancellationToken);
    Task<string?> PickExportDestinationAsync(string suggestedFileName, CancellationToken cancellationToken);
}
