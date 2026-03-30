using Librova.UI.LibraryCatalog;
using Librova.UI.Mvvm;
using System;
using System.Collections.ObjectModel;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class LibraryBrowserViewModel : ObservableObject
{
    private readonly ILibraryCatalogService _libraryCatalogService;
    private string _searchText = string.Empty;
    private string _authorFilter = string.Empty;
    private string _languageFilter = string.Empty;
    private string _statusText = "Library view is idle.";
    private string _selectedFormatFilter = "All";
    private BookSortModel _selectedSort = BookSortModel.DateAdded;
    private bool _isBusy;
    private int _pageSize = 25;
    private int _currentPage = 1;
    private bool _hasMoreResults;
    private BookListItemModel? _selectedBook;

    public LibraryBrowserViewModel(ILibraryCatalogService? libraryCatalogService = null)
    {
        _libraryCatalogService = libraryCatalogService ?? new NullLibraryCatalogService();
        RefreshCommand = new AsyncCommand(RefreshAsync, () => !IsBusy, HandleCommandErrorAsync);
        NextPageCommand = new AsyncCommand(NextPageAsync, () => !IsBusy && HasMoreResults, HandleCommandErrorAsync);
        PreviousPageCommand = new AsyncCommand(PreviousPageAsync, () => !IsBusy && CanGoToPreviousPage, HandleCommandErrorAsync);
    }

    public ObservableCollection<BookListItemModel> Books { get; } = [];
    public AsyncCommand RefreshCommand { get; }
    public AsyncCommand NextPageCommand { get; }
    public AsyncCommand PreviousPageCommand { get; }
    public IReadOnlyList<string> AvailableFormatFilters { get; } = ["All", "EPUB", "FB2"];
    public IReadOnlyList<BookSortModel> AvailableSortOptions { get; } = Enum.GetValues<BookSortModel>();

    public string SearchText
    {
        get => _searchText;
        set => SetProperty(ref _searchText, value);
    }

    public string AuthorFilter
    {
        get => _authorFilter;
        set => SetProperty(ref _authorFilter, value);
    }

    public string LanguageFilter
    {
        get => _languageFilter;
        set => SetProperty(ref _languageFilter, value);
    }

    public string SelectedFormatFilter
    {
        get => _selectedFormatFilter;
        set => SetProperty(ref _selectedFormatFilter, value);
    }

    public BookSortModel SelectedSort
    {
        get => _selectedSort;
        set => SetProperty(ref _selectedSort, value);
    }

    public int PageSize
    {
        get => _pageSize;
        set
        {
            var effectiveValue = value < 1 ? 1 : value;
            if (SetProperty(ref _pageSize, effectiveValue))
            {
                RaisePropertyChanged(nameof(PageSizeText));
            }
        }
    }

    public int CurrentPage
    {
        get => _currentPage;
        private set
        {
            if (SetProperty(ref _currentPage, value))
            {
                RaisePropertyChanged(nameof(PageLabelText));
                RaisePropertyChanged(nameof(CanGoToPreviousPage));
                PreviousPageCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public bool HasMoreResults
    {
        get => _hasMoreResults;
        private set
        {
            if (SetProperty(ref _hasMoreResults, value))
            {
                RaisePropertyChanged(nameof(PageLabelText));
                NextPageCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public BookListItemModel? SelectedBook
    {
        get => _selectedBook;
        set
        {
            if (SetProperty(ref _selectedBook, value))
            {
                RaisePropertyChanged(nameof(HasSelectedBook));
                RaisePropertyChanged(nameof(SelectedBookTitle));
                RaisePropertyChanged(nameof(SelectedBookDetailsText));
                RaisePropertyChanged(nameof(SelectedBookPathText));
            }
        }
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
                NextPageCommand.RaiseCanExecuteChanged();
                PreviousPageCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public bool HasBooks => Books.Count > 0;
    public string BookCountText => $"{Books.Count} book(s)";
    public string PageSizeText => $"{PageSize} per page";
    public string PageLabelText => HasMoreResults ? $"Page {CurrentPage}+" : $"Page {CurrentPage}";
    public bool CanGoToPreviousPage => CurrentPage > 1;
    public bool HasSelectedBook => SelectedBook is not null;
    public string SelectedBookTitle => SelectedBook?.Title ?? "No book selected";
    public string SelectedBookDetailsText =>
        SelectedBook is null
            ? "Select a book from the library snapshot to inspect its metadata."
            : BuildSelectedBookDetailsText(SelectedBook);
    public string SelectedBookPathText => SelectedBook?.ManagedPath ?? "No managed file path available.";

    public async Task RefreshAsync()
    {
        await RefreshPageAsync(1);
    }

    public async Task NextPageAsync()
    {
        if (!HasMoreResults)
        {
            return;
        }

        await RefreshPageAsync(CurrentPage + 1);
    }

    public async Task PreviousPageAsync()
    {
        if (!CanGoToPreviousPage)
        {
            return;
        }

        await RefreshPageAsync(CurrentPage - 1);
    }

    private async Task RefreshPageAsync(int pageNumber)
    {
        IsBusy = true;
        StatusText = pageNumber == CurrentPage
            ? "Refreshing library list..."
            : $"Loading page {pageNumber}...";
        var previousSelectionId = SelectedBook?.BookId;

        try
        {
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(10));
            var items = await _libraryCatalogService.ListBooksAsync(
                BuildRequest(pageNumber),
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            var visibleItems = items.Take(PageSize).ToArray();

            Books.Clear();
            foreach (var item in visibleItems)
            {
                Books.Add(item);
            }

            CurrentPage = pageNumber;
            HasMoreResults = items.Count > PageSize;
            SelectedBook = visibleItems.FirstOrDefault(item => item.BookId == previousSelectionId) ?? visibleItems.FirstOrDefault();
            RaisePropertyChanged(nameof(HasBooks));
            RaisePropertyChanged(nameof(BookCountText));
            StatusText = Books.Count == 0
                ? "No books found for the current filter."
                : $"Loaded {Books.Count} book(s) for page {CurrentPage}.";
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

    private BookListRequestModel BuildRequest(int pageNumber) =>
        new()
        {
            Text = SearchText,
            Author = string.IsNullOrWhiteSpace(AuthorFilter) ? null : AuthorFilter,
            Language = string.IsNullOrWhiteSpace(LanguageFilter) ? null : LanguageFilter,
            Format = SelectedFormatFilter switch
            {
                "EPUB" => BookFormatModel.Epub,
                "FB2" => BookFormatModel.Fb2,
                _ => null
            },
            SortBy = SelectedSort,
            Offset = checked((ulong)Math.Max(0, pageNumber - 1) * (ulong)PageSize),
            Limit = checked((ulong)PageSize + 1UL)
        };

    private static string BuildSelectedBookDetailsText(BookListItemModel item)
    {
        var parts = new List<string>
        {
            item.AuthorsText,
            $"Format: {item.FormatText}",
            $"Language: {item.Language}"
        };

        if (!string.IsNullOrWhiteSpace(item.Series))
        {
            parts.Add(item.SeriesIndex is null
                ? $"Series: {item.Series}"
                : $"Series: {item.Series} #{item.SeriesIndex:0.##}");
        }

        if (item.Year is not null)
        {
            parts.Add($"Year: {item.Year}");
        }

        parts.Add($"Tags: {item.TagsText}");
        parts.Add($"Size: {item.SizeBytes} bytes");
        parts.Add($"Added: {item.AddedAtUtc:yyyy-MM-dd HH:mm} UTC");
        return string.Join(Environment.NewLine, parts);
    }
}
