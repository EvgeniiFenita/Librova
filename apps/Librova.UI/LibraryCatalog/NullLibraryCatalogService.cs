using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.LibraryCatalog;

internal sealed class NullLibraryCatalogService : ILibraryCatalogService
{
    public Task<IReadOnlyList<BookListItemModel>> ListBooksAsync(
        BookListRequestModel request,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        Task.FromResult<IReadOnlyList<BookListItemModel>>([]);

    public Task<BookDetailsModel?> GetBookDetailsAsync(
        long bookId,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        Task.FromResult<BookDetailsModel?>(null);
}
