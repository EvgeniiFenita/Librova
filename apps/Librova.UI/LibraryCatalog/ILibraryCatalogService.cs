using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.LibraryCatalog;

internal interface ILibraryCatalogService
{
    Task<BookListPageModel> ListBooksAsync(
        BookListRequestModel request,
        TimeSpan timeout,
        CancellationToken cancellationToken);

    Task<BookDetailsModel?> GetBookDetailsAsync(
        long bookId,
        TimeSpan timeout,
        CancellationToken cancellationToken);

    Task<IReadOnlyList<BookCollectionModel>> ListCollectionsAsync(
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        Task.FromResult<IReadOnlyList<BookCollectionModel>>([]);

    Task<BookCollectionModel?> CreateCollectionAsync(
        string name,
        string iconKey,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        Task.FromResult<BookCollectionModel?>(null);

    Task<bool> DeleteCollectionAsync(
        long collectionId,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        Task.FromResult(false);

    Task<bool> AddBookToCollectionAsync(
        long bookId,
        long collectionId,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        Task.FromResult(false);

    Task<bool> RemoveBookFromCollectionAsync(
        long bookId,
        long collectionId,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        Task.FromResult(false);

    Task<string?> ExportBookAsync(
        long bookId,
        string destinationPath,
        BookFormatModel? exportFormat,
        TimeSpan timeout,
        CancellationToken cancellationToken);

    Task<DeleteBookResultModel?> MoveBookToTrashAsync(
        long bookId,
        TimeSpan timeout,
        CancellationToken cancellationToken);
}
