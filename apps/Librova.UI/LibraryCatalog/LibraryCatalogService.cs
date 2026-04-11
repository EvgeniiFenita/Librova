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

    public async Task<BookListPageModel> ListBooksAsync(
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

            var result = LibraryCatalogMapper.FromProto(response);
            UiLogging.Information(
                "ListBooksAsync completed. QueryText={QueryText} ReturnedCount={ReturnedCount} TotalCount={TotalCount}",
                request.Text,
                result.Items.Count,
                result.TotalCount);
            return result;
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
            var result = LibraryCatalogMapper.FromProto(response);
            UiLogging.Information(
                "GetBookDetailsAsync completed. BookId={BookId} Found={Found}",
                bookId,
                result is not null);
            return result;
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to load book details. BookId={BookId}", bookId);
            throw;
        }
    }

    public async Task<LibraryStatisticsModel> GetLibraryStatisticsAsync(
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        try
        {
            var response = await _client.GetLibraryStatisticsAsync(timeout, cancellationToken).ConfigureAwait(false);
            var result = LibraryCatalogMapper.FromProto(response);
            UiLogging.Information(
                "GetLibraryStatisticsAsync completed. BookCount={BookCount} TotalLibrarySizeBytes={TotalLibrarySizeBytes}",
                result.BookCount,
                result.TotalLibrarySizeBytes);
            return result;
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to load library statistics.");
            throw;
        }
    }

    public async Task<string?> ExportBookAsync(
        long bookId,
        string destinationPath,
        BookFormatModel? exportFormat,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        try
        {
            var response = await _client.ExportBookAsync(bookId, destinationPath, exportFormat, timeout, cancellationToken).ConfigureAwait(false);
            var result = LibraryCatalogMapper.FromProto(response);
            UiLogging.Information(
                "ExportBookAsync completed. BookId={BookId} ExportedPath={ExportedPath}",
                bookId,
                result ?? "<none>");
            return result;
        }
        catch (Exception error)
        {
            UiLogging.Error(
                error,
                "Failed to export book. BookId={BookId} DestinationPath={DestinationPath} ExportFormat={ExportFormat}",
                bookId,
                destinationPath,
                exportFormat?.ToString() ?? "original");
            throw;
        }
    }

    public async Task<DeleteBookResultModel?> MoveBookToTrashAsync(
        long bookId,
        TimeSpan timeout,
        CancellationToken cancellationToken)
    {
        try
        {
            var response = await _client.MoveBookToTrashAsync(bookId, timeout, cancellationToken).ConfigureAwait(false);
            var result = LibraryCatalogMapper.FromProto(response);
            UiLogging.Information(
                "MoveBookToTrashAsync completed. BookId={BookId} Deleted={Deleted} Destination={Destination} HasOrphanedFiles={HasOrphanedFiles}",
                bookId,
                result is not null,
                result?.Destination.ToString() ?? "<none>",
                result?.HasOrphanedFiles ?? false);
            return result;
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to move book to Recycle Bin. BookId={BookId}", bookId);
            throw;
        }
    }
}
