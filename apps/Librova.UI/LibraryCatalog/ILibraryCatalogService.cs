using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.LibraryCatalog;

internal interface ILibraryCatalogService
{
    Task<IReadOnlyList<BookListItemModel>> ListBooksAsync(
        BookListRequestModel request,
        TimeSpan timeout,
        CancellationToken cancellationToken);

    Task<BookDetailsModel?> GetBookDetailsAsync(
        long bookId,
        TimeSpan timeout,
        CancellationToken cancellationToken);

    Task<string?> ExportBookAsync(
        long bookId,
        string destinationPath,
        TimeSpan timeout,
        CancellationToken cancellationToken);
}
