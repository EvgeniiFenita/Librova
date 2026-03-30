using Librova.V1;
using Librova.UI.PipeTransport;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.LibraryCatalog;

internal sealed class LibraryCatalogClient
{
    private readonly NamedPipeClient _pipeClient;

    public LibraryCatalogClient(string pipePath)
    {
        _pipeClient = new NamedPipeClient(pipePath);
    }

    public Task<ListBooksResponse> ListBooksAsync(
        BookListRequest request,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        _pipeClient.CallAsync(
            PipeMethod.ListBooks,
            new ListBooksRequest { Query = request },
            ListBooksResponse.Parser,
            timeout,
            cancellationToken);
}
