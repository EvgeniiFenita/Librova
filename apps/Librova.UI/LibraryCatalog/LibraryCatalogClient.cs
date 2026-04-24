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

    public Task<ListCollectionsResponse> ListCollectionsAsync(
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        _pipeClient.CallAsync(
            PipeMethod.ListCollections,
            new ListCollectionsRequest(),
            ListCollectionsResponse.Parser,
            timeout,
            cancellationToken);

    public Task<CreateCollectionResponse> CreateCollectionAsync(
        string name,
        string iconKey,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        _pipeClient.CallAsync(
            PipeMethod.CreateCollection,
            new CreateCollectionRequest
            {
                Name = name,
                IconKey = iconKey
            },
            CreateCollectionResponse.Parser,
            timeout,
            cancellationToken);

    public Task<DeleteCollectionResponse> DeleteCollectionAsync(
        long collectionId,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        _pipeClient.CallAsync(
            PipeMethod.DeleteCollection,
            new DeleteCollectionRequest
            {
                CollectionId = collectionId
            },
            DeleteCollectionResponse.Parser,
            timeout,
            cancellationToken);

    public Task<AddBookToCollectionResponse> AddBookToCollectionAsync(
        long bookId,
        long collectionId,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        _pipeClient.CallAsync(
            PipeMethod.AddBookToCollection,
            new AddBookToCollectionRequest
            {
                BookId = bookId,
                CollectionId = collectionId
            },
            AddBookToCollectionResponse.Parser,
            timeout,
            cancellationToken);

    public Task<RemoveBookFromCollectionResponse> RemoveBookFromCollectionAsync(
        long bookId,
        long collectionId,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        _pipeClient.CallAsync(
            PipeMethod.RemoveBookFromCollection,
            new RemoveBookFromCollectionRequest
            {
                BookId = bookId,
                CollectionId = collectionId
            },
            RemoveBookFromCollectionResponse.Parser,
            timeout,
            cancellationToken);

    public Task<ExportBookResponse> ExportBookAsync(
        long bookId,
        string destinationPath,
        BookFormatModel? exportFormat,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        _pipeClient.CallAsync(
            PipeMethod.ExportBook,
            new ExportBookRequest
            {
                BookId = bookId,
                DestinationPath = destinationPath,
                ExportFormat = exportFormat switch
                {
                    BookFormatModel.Fb2 => BookFormat.Fb2,
                    BookFormatModel.Epub => BookFormat.Epub,
                    _ => BookFormat.Unspecified
                }
            },
            ExportBookResponse.Parser,
            timeout,
            cancellationToken);

    public Task<MoveBookToTrashResponse> MoveBookToTrashAsync(
        long bookId,
        TimeSpan timeout,
        CancellationToken cancellationToken) =>
        _pipeClient.CallAsync(
            PipeMethod.MoveBookToTrash,
            new MoveBookToTrashRequest
            {
                BookId = bookId
            },
            MoveBookToTrashResponse.Parser,
            timeout,
            cancellationToken);
}
