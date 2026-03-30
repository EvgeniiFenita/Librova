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

    public async Task<BookDetailsModel?> GetBookDetailsAsync(
        long bookId,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        try
        {
            var response = await _client.GetBookDetailsAsync(bookId, timeout, cancellationToken).ConfigureAwait(false);
            return LibraryCatalogMapper.FromProto(response);
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to load book details. BookId={BookId}", bookId);
            throw;
        }
    }

    public async Task<string?> ExportBookAsync(
        long bookId,
        string destinationPath,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        try
        {
            var response = await _client.ExportBookAsync(bookId, destinationPath, timeout, cancellationToken).ConfigureAwait(false);
            return LibraryCatalogMapper.FromProto(response);
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to export book. BookId={BookId} DestinationPath={DestinationPath}", bookId, destinationPath);
            throw;
        }
    }

    public async Task<bool> MoveBookToTrashAsync(
        long bookId,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        try
        {
            var response = await _client.MoveBookToTrashAsync(bookId, timeout, cancellationToken).ConfigureAwait(false);
            return LibraryCatalogMapper.FromProto(response);
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to move book to trash. BookId={BookId}", bookId);
            throw;
        }
    }
}
