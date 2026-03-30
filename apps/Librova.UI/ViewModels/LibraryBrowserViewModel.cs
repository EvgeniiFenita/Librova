using Librova.UI.LibraryCatalog;
using Librova.UI.Mvvm;
using System;
using System.Collections.ObjectModel;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class LibraryBrowserViewModel : ObservableObject
{
    private readonly ILibraryCatalogService _libraryCatalogService;
    private string _searchText = string.Empty;
    private string _statusText = "Library view is idle.";
    private bool _isBusy;

    public LibraryBrowserViewModel(ILibraryCatalogService? libraryCatalogService = null)
    {
        _libraryCatalogService = libraryCatalogService ?? new NullLibraryCatalogService();
        RefreshCommand = new AsyncCommand(RefreshAsync, () => !IsBusy, HandleCommandErrorAsync);
    }

    public ObservableCollection<BookListItemModel> Books { get; } = [];
    public AsyncCommand RefreshCommand { get; }

    public string SearchText
    {
        get => _searchText;
        set => SetProperty(ref _searchText, value);
    }

    public string StatusText
    {
        get => _statusText;
        private set => SetProperty(ref _statusText, value);
    }

    public bool IsBusy
    {
        get => _isBusy;
        private set
        {
            if (SetProperty(ref _isBusy, value))
            {
                RefreshCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public bool HasBooks => Books.Count > 0;
    public string BookCountText => $"{Books.Count} book(s)";

    public async Task RefreshAsync()
    {
        IsBusy = true;
        StatusText = "Refreshing library list...";

        try
        {
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(10));
            var items = await _libraryCatalogService.ListBooksAsync(
                new BookListRequestModel
                {
                    Text = SearchText,
                    SortBy = BookSortModel.DateAdded,
                    Limit = 25
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Books.Clear();
            foreach (var item in items)
            {
                Books.Add(item);
            }

            RaisePropertyChanged(nameof(HasBooks));
            RaisePropertyChanged(nameof(BookCountText));
            StatusText = Books.Count == 0
                ? "No books found for the current filter."
                : $"Loaded {Books.Count} book(s).";
        }
        finally
        {
            IsBusy = false;
        }
    }

    private Task HandleCommandErrorAsync(Exception error)
    {
        StatusText = string.IsNullOrWhiteSpace(error.Message) ? "Failed to refresh library list." : error.Message;
        return Task.CompletedTask;
    }
}
