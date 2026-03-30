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

    public Task<GetBookDetailsResponse> GetBookDetailsAsync(
        long bookId,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        _pipeClient.CallAsync(
            PipeMethod.GetBookDetails,
            new GetBookDetailsRequest { BookId = bookId },
            GetBookDetailsResponse.Parser,
            timeout,
            cancellationToken);

    public Task<ExportBookResponse> ExportBookAsync(
        long bookId,
        string destinationPath,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        _pipeClient.CallAsync(
            PipeMethod.ExportBook,
            new ExportBookRequest
            {
                BookId = bookId,
                DestinationPath = destinationPath
            },
            ExportBookResponse.Parser,
            timeout,
            cancellationToken);
}
