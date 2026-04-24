using Librova.UI.LibraryCatalog;
using Librova.UI.ViewModels;
using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Xunit;

namespace Librova.UI.Tests;

public sealed class LibraryCollectionsViewModelTests
{
    [Fact]
    public async Task LibraryBrowserViewModel_SelectingCollectionRefreshesWithCollectionFilter()
    {
        var service = new CollectionAwareLibraryCatalogService();
        long? activatedCollectionId = null;
        var viewModel = new LibraryBrowserViewModel(
            service,
            activateCollectionNavigation: collectionId => activatedCollectionId = collectionId);

        await viewModel.RefreshAsync();

        Assert.Equal(2, viewModel.Collections.Count);
        Assert.Null(service.LastRequest?.CollectionId);

        await viewModel.SelectCollectionCommand.ExecuteAsyncForTests(viewModel.Collections[0]);

        Assert.True(viewModel.IsCollectionViewActive);
        Assert.Equal(viewModel.Collections[0].CollectionId, activatedCollectionId);
        Assert.Equal(viewModel.Collections[0].CollectionId, service.LastRequest?.CollectionId);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_ClearAllFiltersPreservesSelectedCollection()
    {
        var service = new CollectionAwareLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();
        await viewModel.SelectCollectionCommand.ExecuteAsyncForTests(viewModel.Collections[0]);

        Assert.True(viewModel.IsCollectionViewActive);
        Assert.Equal(0, viewModel.ActiveFilterCount);

        await viewModel.ClearAllFiltersCommand.ExecuteAsyncForTests();
        await viewModel.RefreshAsync();

        Assert.True(viewModel.IsCollectionViewActive);
        Assert.Equal(0, viewModel.ActiveFilterCount);
        Assert.Equal(viewModel.Collections[0].CollectionId, service.LastRequest?.CollectionId);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_CreateCollectionForBookAddsMembershipAfterCreate()
    {
        var service = new CollectionAwareLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();

        viewModel.StartCreateCollectionForBook(viewModel.Books[0]);
        viewModel.NewCollectionName = "Favorites";

        await viewModel.CreateCollectionCommand.ExecuteAsyncForTests();

        Assert.Equal("Favorites", service.CreatedCollectionName);
        Assert.Equal(viewModel.Books[0].BookId, service.LastAddedBookId);
        Assert.Equal(service.CreatedCollectionId, service.LastAddedCollectionId);
        Assert.False(viewModel.IsCreateCollectionDialogOpen);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_CreateCollectionForBookRollsBackWhenMembershipAddReturnsFalse()
    {
        var service = new CollectionAwareLibraryCatalogService
        {
            AddBookToCollectionResult = false
        };
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();

        viewModel.StartCreateCollectionForBook(viewModel.Books[0]);
        viewModel.NewCollectionName = "Favorites";

        await viewModel.CreateCollectionCommand.ExecuteAsyncForTests();

        Assert.Equal(service.CreatedCollectionId, service.LastDeletedCollectionId);
        Assert.True(viewModel.IsCreateCollectionDialogOpen);
        Assert.Contains("could not add", viewModel.StatusText, StringComparison.Ordinal);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_CreateCollectionForBookRollsBackWhenMembershipAddThrows()
    {
        var service = new CollectionAwareLibraryCatalogService
        {
            ThrowOnAddBookToCollection = true
        };
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();

        viewModel.StartCreateCollectionForBook(viewModel.Books[0]);
        viewModel.NewCollectionName = "Favorites";

        await viewModel.CreateCollectionCommand.ExecuteAsyncForTests();

        Assert.Equal(service.CreatedCollectionId, service.LastDeletedCollectionId);
        Assert.True(viewModel.IsCreateCollectionDialogOpen);
        Assert.Contains("Failed to add book to collection", viewModel.StatusText, StringComparison.Ordinal);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_AddToCollectionDoesNotRemoveExistingMembership()
    {
        var service = new CollectionAwareLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();

        var book = viewModel.Books[0];
        var collection = viewModel.Collections[0].Collection;

        await viewModel.ToggleBookCollectionMembershipCommand.ExecuteAsyncForTests(
            new BookCollectionMembershipRequest(book, collection, IsMember: false));
        Assert.Equal(book.BookId, service.LastAddedBookId);
        Assert.Equal(collection.CollectionId, service.LastAddedCollectionId);
        Assert.Null(service.LastRemovedBookId);

        await viewModel.ToggleBookCollectionMembershipCommand.ExecuteAsyncForTests(
            new BookCollectionMembershipRequest(book, collection, IsMember: true));
        Assert.Null(service.LastRemovedBookId);
        Assert.Null(service.LastRemovedCollectionId);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_AddToCollectionUsesServiceResultForStatusText()
    {
        var service = new CollectionAwareLibraryCatalogService
        {
            AddBookToCollectionResult = false
        };
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();

        var book = viewModel.Books[0];
        var collection = viewModel.Collections[0].Collection;

        await viewModel.ToggleBookCollectionMembershipCommand.ExecuteAsyncForTests(
            new BookCollectionMembershipRequest(book, collection, IsMember: false));

        Assert.Contains("already in", viewModel.StatusText, StringComparison.Ordinal);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_RemoveFromSelectedCollectionUsesOriginalCollectionName()
    {
        var service = new CollectionAwareLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();
        await viewModel.SelectCollectionCommand.ExecuteAsyncForTests(viewModel.Collections[0]);

        service.OnRemoveBookFromCollectionAsync = () =>
            viewModel.SelectCollectionCommand.ExecuteAsyncForTests(viewModel.Collections[1]);

        await viewModel.RemoveBookFromSelectedCollectionCommand.ExecuteAsyncForTests(viewModel.Books[0]);

        Assert.Contains("Favorites", viewModel.StatusText, StringComparison.Ordinal);
    }

    private sealed class CollectionAwareLibraryCatalogService : ILibraryCatalogService
    {
        public BookListRequestModel? LastRequest { get; private set; }
        public string? CreatedCollectionName { get; private set; }
        public long CreatedCollectionId { get; private set; } = 50;
        public bool AddBookToCollectionResult { get; init; } = true;
        public bool ThrowOnAddBookToCollection { get; init; }
        public long? LastAddedBookId { get; private set; }
        public long? LastAddedCollectionId { get; private set; }
        public long? LastRemovedBookId { get; private set; }
        public long? LastRemovedCollectionId { get; private set; }
        public long? LastDeletedCollectionId { get; private set; }
        public Func<Task>? OnRemoveBookFromCollectionAsync { get; set; }

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            LastRequest = request;
            return Task.FromResult(new BookListPageModel
            {
                Items =
                [
                    new BookListItemModel
                    {
                        BookId = 7,
                        Title = "Alpha",
                        Authors = ["Author"],
                        Language = "en",
                        Format = BookFormatModel.Epub,
                        ManagedPath = "Objects/aa/aa/0000000007.book.epub",
                        SizeBytes = 1024,
                        AddedAtUtc = DateTimeOffset.UtcNow
                    }
                ],
                TotalCount = 1,
                Statistics = new LibraryStatisticsModel
                {
                    BookCount = 1,
                    TotalLibrarySizeBytes = 1024
                }
            });
        }

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken) =>
            Task.FromResult<BookDetailsModel?>(new BookDetailsModel
            {
                BookId = bookId,
                Title = "Alpha",
                Authors = ["Author"],
                Language = "en",
                Format = BookFormatModel.Epub,
                ManagedPath = "Objects/aa/aa/0000000007.book.epub",
                SizeBytes = 1024,
                AddedAtUtc = DateTimeOffset.UtcNow,
                Collections =
                [
                    new(10, "Favorites", "star", BookCollectionKindModel.User, true)
                ]
            });

        public Task<IReadOnlyList<BookCollectionModel>> ListCollectionsAsync(TimeSpan timeout, CancellationToken cancellationToken) =>
            Task.FromResult<IReadOnlyList<BookCollectionModel>>(
            [
                new(10, "Favorites", "star", BookCollectionKindModel.User, true),
                new(11, "Unread", "bookmark", BookCollectionKindModel.User, true)
            ]);

        public Task<BookCollectionModel?> CreateCollectionAsync(string name, string iconKey, TimeSpan timeout, CancellationToken cancellationToken)
        {
            CreatedCollectionName = name;
            return Task.FromResult<BookCollectionModel?>(new BookCollectionModel(CreatedCollectionId, name, iconKey, BookCollectionKindModel.User, true));
        }

        public Task<bool> DeleteCollectionAsync(long collectionId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            LastDeletedCollectionId = collectionId;
            return Task.FromResult(true);
        }

        public Task<bool> AddBookToCollectionAsync(long bookId, long collectionId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            if (ThrowOnAddBookToCollection)
            {
                throw new InvalidOperationException("Failed to add book to collection.");
            }

            LastAddedBookId = bookId;
            LastAddedCollectionId = collectionId;
            return Task.FromResult(AddBookToCollectionResult);
        }

        public Task<bool> RemoveBookFromCollectionAsync(long bookId, long collectionId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            LastRemovedBookId = bookId;
            LastRemovedCollectionId = collectionId;
            return RemoveBookFromCollectionCoreAsync();
        }

        private async Task<bool> RemoveBookFromCollectionCoreAsync()
        {
            if (OnRemoveBookFromCollectionAsync is not null)
            {
                await OnRemoveBookFromCollectionAsync();
            }

            return true;
        }

        public Task<string?> ExportBookAsync(long bookId, string destinationPath, BookFormatModel? exportFormat, TimeSpan timeout, CancellationToken cancellationToken) =>
            Task.FromResult<string?>(null);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken) =>
            Task.FromResult<DeleteBookResultModel?>(null);
    }
}
