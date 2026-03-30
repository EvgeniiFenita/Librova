using Librova.UI.Logging;
using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.LibraryCatalog;

internal sealed class LibraryCatalogService : ILibraryCatalogService
{
    private readonly LibraryCatalogClient _client;

    public LibraryCatalogService(string pipePath)
        : this(new LibraryCatalogClient(pipePath))
    {
    }

    public LibraryCatalogService(LibraryCatalogClient client)
    {
        _client = client;
    }

    public async Task<IReadOnlyList<BookListItemModel>> ListBooksAsync(
        BookListRequestModel request,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        try
        {
            var response = await _client.ListBooksAsync(
                LibraryCatalogMapper.ToProto(request),
                timeout,
                cancellationToken).ConfigureAwait(false);

            return LibraryCatalogMapper.FromProto(response);
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to list books. QueryText={QueryText}", request.Text);
            throw;
        }
    }
}
