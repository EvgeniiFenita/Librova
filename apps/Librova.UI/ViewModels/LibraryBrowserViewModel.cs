using Avalonia;
using Avalonia.Media;
using Librova.UI.Desktop;
using Librova.UI.LibraryCatalog;
using Librova.UI.Mvvm;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class LibraryBrowserViewModel : ObservableObject
{
    private static readonly IBrush DefaultCardBackground = new SolidColorBrush(Color.Parse("#FFF9F1"));
    private static readonly IBrush DefaultCardBorder = new SolidColorBrush(Color.Parse("#D8CCBD"));
    private static readonly IBrush SelectedCardBackground = new SolidColorBrush(Color.Parse("#F7E9D8"));
    private static readonly IBrush SelectedCardBorder = new SolidColorBrush(Color.Parse("#BA5A2A"));
    private readonly ILibraryCatalogService _libraryCatalogService;
    private readonly IPathSelectionService _pathSelectionService;
    private readonly ICoverImageLoader _coverImageLoader;
    private readonly string _libraryRoot;
    private CancellationTokenSource? _refreshDebounce;
    private string _searchText = string.Empty;
    private string _languageFilter = string.Empty;
    private string _statusText = "Library view is idle.";
    private bool _isBusy;
    private bool _isLoadingSelectionDetails;
    private int _pageSize = 120;
    private int _currentPage = 1;
    private bool _hasMoreResults;
    private BookListItemModel? _selectedBook;
    private BookDetailsModel? _selectedBookDetails;

    public LibraryBrowserViewModel(
        ILibraryCatalogService? libraryCatalogService = null,
        IPathSelectionService? pathSelectionService = null,
        string? libraryRoot = null,
        ICoverImageLoader? coverImageLoader = null)
    {
        _libraryCatalogService = libraryCatalogService ?? new NullLibraryCatalogService();
        _pathSelectionService = pathSelectionService ?? new NullPathSelectionService();
        _coverImageLoader = coverImageLoader ?? new FileCoverImageLoader();
        _libraryRoot = libraryRoot ?? string.Empty;
        RefreshCommand = new AsyncCommand(RefreshAsync, () => !IsBusy, HandleCommandErrorAsync);
        NextPageCommand = new AsyncCommand(NextPageAsync, () => !IsBusy && HasMoreResults, HandleCommandErrorAsync);
        PreviousPageCommand = new AsyncCommand(PreviousPageAsync, () => !IsBusy && CanGoToPreviousPage, HandleCommandErrorAsync);
        LoadDetailsCommand = new AsyncCommand(LoadSelectedBookDetailsAsync, () => HasSelectedBook && !IsLoadingSelectionDetails, HandleCommandErrorAsync);
        ExportSelectedBookCommand = new AsyncCommand(ExportSelectedBookAsync, () => !IsBusy && HasSelectedBook, HandleCommandErrorAsync);
        MoveSelectedBookToTrashCommand = new AsyncCommand(MoveSelectedBookToTrashAsync, () => !IsBusy && HasSelectedBook, HandleCommandErrorAsync);
        SelectBookCommand = new AsyncCommand<BookListItemModel?>(ToggleSelectedBookAsync, book => book is not null);
        CloseSelectionCommand = new AsyncCommand(CloseSelectionAsync, () => HasSelectedBook, HandleCommandErrorAsync);
        AvailableLanguageFilters.Add("All languages");
    }

    public ObservableCollection<BookListItemModel> Books { get; } = [];
    public ObservableCollection<string> AvailableLanguageFilters { get; } = [];
    public AsyncCommand RefreshCommand { get; }
    public AsyncCommand NextPageCommand { get; }
    public AsyncCommand PreviousPageCommand { get; }
    public AsyncCommand LoadDetailsCommand { get; }
    public AsyncCommand ExportSelectedBookCommand { get; }
    public AsyncCommand MoveSelectedBookToTrashCommand { get; }
    public AsyncCommand<BookListItemModel?> SelectBookCommand { get; }
    public AsyncCommand CloseSelectionCommand { get; }

    public string SearchText
    {
        get => _searchText;
        set
        {
            if (SetProperty(ref _searchText, value))
            {
                ScheduleRefresh();
            }
        }
    }

    public string LanguageFilter
    {
        get => _languageFilter;
        set
        {
            if (SetProperty(ref _languageFilter, value))
            {
                RaisePropertyChanged(nameof(SelectedLanguageFilter));
                RaisePropertyChanged(nameof(BookCountText));
                RaisePropertyChanged(nameof(EmptyStateTitle));
                RaisePropertyChanged(nameof(EmptyStateDescription));
                ScheduleRefresh();
            }
        }
    }

    public string SelectedLanguageFilter
    {
        get => string.IsNullOrWhiteSpace(LanguageFilter) ? "All languages" : LanguageFilter;
        set
        {
            var effectiveValue = string.Equals(value, "All languages", StringComparison.OrdinalIgnoreCase)
                ? string.Empty
                : value;
            LanguageFilter = effectiveValue;
        }
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
        private set
        {
            if (ReferenceEquals(_selectedBook, value))
            {
                return;
            }

            if (_selectedBook is not null)
            {
                _selectedBook.IsSelected = false;
                ApplySelectionStyle(_selectedBook, isSelected: false);
            }

            if (SetProperty(ref _selectedBook, value))
            {
                if (_selectedBook is not null)
                {
                    _selectedBook.IsSelected = true;
                    ApplySelectionStyle(_selectedBook, isSelected: true);
                }

                SelectedBookDetails = null;
                RaisePropertyChanged(nameof(HasSelectedBook));
                RaisePropertyChanged(nameof(ShowDetailsPanel));
                RaisePropertyChanged(nameof(EmptyStateTitle));
                RaisePropertyChanged(nameof(EmptyStateDescription));
                RaisePropertyChanged(nameof(SelectedBookTitle));
                RaisePropertyChanged(nameof(SelectedBookAuthorText));
                RaisePropertyChanged(nameof(SelectedBookMetadataPairs));
                RaisePropertyChanged(nameof(SelectedBookAnnotationText));
                RaisePropertyChanged(nameof(SelectedBookPathText));
                RaisePropertyChanged(nameof(SelectedBookCoverImage));
                RaisePropertyChanged(nameof(SelectedBookCoverBackgroundBrush));
                RaisePropertyChanged(nameof(SelectedBookCoverPlaceholderText));
                RaisePropertyChanged(nameof(HasSelectedBookCover));
                RaisePropertyChanged(nameof(ShowSelectedBookCoverPlaceholder));
                LoadDetailsCommand.RaiseCanExecuteChanged();
                ExportSelectedBookCommand.RaiseCanExecuteChanged();
                MoveSelectedBookToTrashCommand.RaiseCanExecuteChanged();
                CloseSelectionCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public BookDetailsModel? SelectedBookDetails
    {
        get => _selectedBookDetails;
        private set
        {
            if (SetProperty(ref _selectedBookDetails, value))
            {
                RaisePropertyChanged(nameof(SelectedBookAuthorText));
                RaisePropertyChanged(nameof(SelectedBookMetadataPairs));
                RaisePropertyChanged(nameof(SelectedBookAnnotationText));
                RaisePropertyChanged(nameof(SelectedBookPathText));
                RaisePropertyChanged(nameof(SelectedBookCoverImage));
                RaisePropertyChanged(nameof(SelectedBookCoverBackgroundBrush));
                RaisePropertyChanged(nameof(SelectedBookCoverPlaceholderText));
                RaisePropertyChanged(nameof(HasSelectedBookCover));
                RaisePropertyChanged(nameof(ShowSelectedBookCoverPlaceholder));
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
                LoadDetailsCommand.RaiseCanExecuteChanged();
                ExportSelectedBookCommand.RaiseCanExecuteChanged();
                MoveSelectedBookToTrashCommand.RaiseCanExecuteChanged();
                SelectBookCommand.RaiseCanExecuteChanged();
                CloseSelectionCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public bool IsLoadingSelectionDetails
    {
        get => _isLoadingSelectionDetails;
        private set
        {
            if (SetProperty(ref _isLoadingSelectionDetails, value))
            {
                RaisePropertyChanged(nameof(ShowDetailsPanelLoading));
                LoadDetailsCommand.RaiseCanExecuteChanged();
            }
        }
    }

    public bool HasBooks => Books.Count > 0;
    public bool HasSelectedBook => SelectedBook is not null;
    public bool ShowDetailsPanel => HasSelectedBook;
    public bool ShowDetailsPanelLoading => HasSelectedBook && IsLoadingSelectionDetails;
    public bool ShowEmptyState => !HasBooks;
    public bool CanGoToPreviousPage => CurrentPage > 1;
    public string PageSizeText => $"{PageSize} per page";
    public string PageLabelText => HasMoreResults ? $"Page {CurrentPage}+" : $"Page {CurrentPage}";
    public string BookCountText => HasActiveFilters
        ? $"{Books.Count:N0} shown"
        : $"{Books.Count:N0} books";
    public string EmptyStateTitle => HasActiveFilters ? "Nothing found" : "Library is empty";
    public string EmptyStateDescription => HasActiveFilters
        ? "Try a different search query or clear the current language filter."
        : "Import books to start building your library.";
    public string SelectedBookTitle => SelectedBookDetails?.Title ?? SelectedBook?.Title ?? "No book selected";
    public string SelectedBookAuthorText =>
        SelectedBookDetails is not null
            ? BuildAuthorsText(SelectedBookDetails.Authors)
            : SelectedBook?.AuthorsText ?? "Choose a book to inspect metadata.";
    public string SelectedBookAnnotationText =>
        string.IsNullOrWhiteSpace(SelectedBookDetails?.Description)
            ? "No annotation available."
            : SelectedBookDetails!.Description!;
    public string SelectedBookPathText =>
        SelectedBookDetails?.ManagedPath
        ?? SelectedBook?.ManagedPath
        ?? "No managed file path available.";
    public IImage? SelectedBookCoverImage => SelectedBookDetails?.ResolvedCoverImage ?? SelectedBook?.ResolvedCoverImage;
    public IBrush? SelectedBookCoverBackgroundBrush => SelectedBookDetails?.CoverBackgroundBrush ?? SelectedBook?.CoverBackgroundBrush;
    public string SelectedBookCoverPlaceholderText => SelectedBookDetails?.CoverPlaceholderText ?? SelectedBook?.CoverPlaceholderText ?? "BOOK";
    public bool HasSelectedBookCover => SelectedBookCoverImage is not null;
    public bool ShowSelectedBookCoverPlaceholder => !HasSelectedBookCover;
    public IReadOnlyList<MetadataPair> SelectedBookMetadataPairs => BuildSelectedBookMetadataPairs();

    private bool HasActiveFilters =>
        !string.IsNullOrWhiteSpace(SearchText)
        || !string.IsNullOrWhiteSpace(LanguageFilter);

    public async Task RefreshAsync()
    {
        await RefreshPageAsync(CurrentPage <= 0 ? 1 : CurrentPage);
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

    public async Task ToggleSelectedBookAsync(BookListItemModel? book)
    {
        if (book is null)
        {
            return;
        }

        if (SelectedBook?.BookId == book.BookId)
        {
            SelectedBook = null;
            StatusText = "Closed book details.";
            return;
        }

        SelectedBook = book;
        StatusText = $"Selected '{book.Title}'.";
        await LoadSelectedBookDetailsAsync();
    }

    public Task CloseSelectionAsync()
    {
        SelectedBook = null;
        StatusText = "Closed book details.";
        return Task.CompletedTask;
    }

    public async Task LoadSelectedBookDetailsAsync()
    {
        if (SelectedBook is null)
        {
            return;
        }

        IsLoadingSelectionDetails = true;
        StatusText = $"Loading details for '{SelectedBook.Title}'...";

        try
        {
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(10));
            var details = await _libraryCatalogService.GetBookDetailsAsync(
                SelectedBook.BookId,
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            SelectedBookDetails = details is null ? null : Prepare(details);
            StatusText = SelectedBookDetails is null
                ? "Book details were not found."
                : $"Loaded details for '{SelectedBookDetails.Title}'.";
        }
        finally
        {
            IsLoadingSelectionDetails = false;
        }
    }

    public async Task ExportSelectedBookAsync()
    {
        if (SelectedBook is null)
        {
            return;
        }

        const string defaultFileName = "book.epub";
        var suggestedFileName = Path.GetFileName(SelectedBook.ManagedPath);
        if (string.IsNullOrWhiteSpace(suggestedFileName))
        {
            suggestedFileName = defaultFileName;
        }

        var destinationPath = await _pathSelectionService.PickExportDestinationAsync(suggestedFileName, default);
        if (string.IsNullOrWhiteSpace(destinationPath))
        {
            StatusText = "Export was cancelled before a destination was chosen.";
            return;
        }

        IsBusy = true;
        StatusText = $"Exporting '{SelectedBook.Title}'...";

        try
        {
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(10));
            var exportedPath = await _libraryCatalogService.ExportBookAsync(
                SelectedBook.BookId,
                destinationPath,
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            StatusText = string.IsNullOrWhiteSpace(exportedPath)
                ? "Selected book could not be exported."
                : $"Exported '{SelectedBook.Title}' to '{exportedPath}'.";
        }
        finally
        {
            IsBusy = false;
        }
    }

    public async Task MoveSelectedBookToTrashAsync()
    {
        if (SelectedBook is null)
        {
            return;
        }

        IsBusy = true;
        StatusText = $"Moving '{SelectedBook.Title}' to trash...";

        try
        {
            var deletedTitle = SelectedBook.Title;
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(10));
            var moved = await _libraryCatalogService.MoveBookToTrashAsync(
                SelectedBook.BookId,
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            if (!moved)
            {
                StatusText = "Selected book could not be moved to trash.";
                return;
            }

            var requestedPage = CurrentPage;
            await RefreshPageAsync(requestedPage);
            if (Books.Count == 0 && requestedPage > 1)
            {
                await RefreshPageAsync(requestedPage - 1);
            }

            StatusText = $"Moved '{deletedTitle}' to trash.";
        }
        finally
        {
            IsBusy = false;
        }
    }

    private void ScheduleRefresh()
    {
        _refreshDebounce?.Cancel();
        _refreshDebounce?.Dispose();
        _refreshDebounce = new CancellationTokenSource();

        _ = RefreshAfterDebounceAsync(_refreshDebounce.Token);
    }

    private async Task RefreshAfterDebounceAsync(CancellationToken cancellationToken)
    {
        try
        {
            await Task.Delay(250, cancellationToken);
            cancellationToken.ThrowIfCancellationRequested();
            await RefreshPageAsync(1);
        }
        catch (OperationCanceledException)
        {
        }
        catch (Exception error)
        {
            await HandleCommandErrorAsync(error);
        }
    }

    private async Task RefreshPageAsync(int pageNumber)
    {
        IsBusy = true;
        StatusText = pageNumber == CurrentPage
            ? "Refreshing library..."
            : $"Loading page {pageNumber}...";
        var previousSelectionId = SelectedBook?.BookId;

        try
        {
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(10));
            var items = await _libraryCatalogService.ListBooksAsync(
                BuildRequest(pageNumber),
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            var visibleItems = items.Take(PageSize).Select(Prepare).ToArray();
            Books.Clear();
            foreach (var item in visibleItems)
            {
                Books.Add(item);
            }

            UpdateAvailableLanguages(visibleItems);
            CurrentPage = pageNumber;
            HasMoreResults = items.Count > PageSize;

            if (previousSelectionId is not null)
            {
                SelectedBook = visibleItems.FirstOrDefault(item => item.BookId == previousSelectionId);
                if (SelectedBook is not null)
                {
                    await LoadSelectedBookDetailsAsync();
                }
            }
            else
            {
                SelectedBook = null;
            }

            RaisePropertyChanged(nameof(HasBooks));
            RaisePropertyChanged(nameof(ShowEmptyState));
            RaisePropertyChanged(nameof(BookCountText));
            RaisePropertyChanged(nameof(EmptyStateTitle));
            RaisePropertyChanged(nameof(EmptyStateDescription));
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
        StatusText = string.IsNullOrWhiteSpace(error.Message) ? "Failed to update the library view." : error.Message;
        return Task.CompletedTask;
    }

    private BookListRequestModel BuildRequest(int pageNumber) =>
        new()
        {
            Text = SearchText,
            Language = string.IsNullOrWhiteSpace(LanguageFilter) ? null : LanguageFilter,
            Offset = checked((ulong)Math.Max(0, pageNumber - 1) * (ulong)PageSize),
            Limit = checked((ulong)PageSize + 1UL)
        };

    private BookListItemModel Prepare(BookListItemModel item)
    {
        item.ResolvedCoverImage = LoadCoverImage(item.CoverPath);
        item.CoverBackgroundBrush = CreateGradientBrush(item.BookId, item.Title, item.AuthorsText);
        item.CoverPlaceholderText = BuildCoverPlaceholderText(item.Title);
        item.IsSelected = false;
        ApplySelectionStyle(item, isSelected: false);
        return item;
    }

    private BookDetailsModel Prepare(BookDetailsModel item)
    {
        item.ResolvedCoverImage = LoadCoverImage(item.CoverPath);
        item.CoverBackgroundBrush = CreateGradientBrush(item.BookId, item.Title, BuildAuthorsText(item.Authors));
        item.CoverPlaceholderText = BuildCoverPlaceholderText(item.Title);
        return item;
    }

    private IImage? LoadCoverImage(string? coverPath)
    {
        if (string.IsNullOrWhiteSpace(coverPath))
        {
            return null;
        }

        try
        {
            var resolvedPath = Path.IsPathFullyQualified(coverPath)
                ? coverPath
                : string.IsNullOrWhiteSpace(_libraryRoot)
                    ? null
                    : Path.GetFullPath(Path.Combine(_libraryRoot, coverPath));

            if (string.IsNullOrWhiteSpace(resolvedPath) || !File.Exists(resolvedPath))
            {
                return null;
            }

            return _coverImageLoader.Load(resolvedPath);
        }
        catch (Exception)
        {
            return null;
        }
    }

    private void UpdateAvailableLanguages(IEnumerable<BookListItemModel> visibleItems)
    {
        var items = new List<string> { "All languages" };
        items.AddRange(visibleItems
            .Select(item => item.Language)
            .Where(language => !string.IsNullOrWhiteSpace(language))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(language => language, StringComparer.OrdinalIgnoreCase));

        if (!string.IsNullOrWhiteSpace(LanguageFilter) && !items.Contains(LanguageFilter, StringComparer.OrdinalIgnoreCase))
        {
            items.Add(LanguageFilter);
        }

        AvailableLanguageFilters.Clear();
        foreach (var item in items.Distinct(StringComparer.OrdinalIgnoreCase))
        {
            AvailableLanguageFilters.Add(item);
        }

        RaisePropertyChanged(nameof(SelectedLanguageFilter));
    }

    private IReadOnlyList<MetadataPair> BuildSelectedBookMetadataPairs()
    {
        if (SelectedBook is null)
        {
            return [];
        }

        var source = SelectedBookDetails;
        var pairs = new List<MetadataPair>();

        if (!string.IsNullOrWhiteSpace(source?.Language ?? SelectedBook.Language))
        {
            pairs.Add(new("Language", source?.Language ?? SelectedBook.Language));
        }

        if (source?.Year is not null || SelectedBook.Year is not null)
        {
            pairs.Add(new("Year", (source?.Year ?? SelectedBook.Year)!.Value.ToString()));
        }

        if (!string.IsNullOrWhiteSpace(source?.Publisher))
        {
            pairs.Add(new("Publisher", source!.Publisher!));
        }

        if (!string.IsNullOrWhiteSpace(source?.Series))
        {
            pairs.Add(new("Series", source!.SeriesIndex is null
                ? source.Series!
                : $"{source.Series} #{source.SeriesIndex:0.##}"));
        }
        else if (!string.IsNullOrWhiteSpace(SelectedBook.Series))
        {
            pairs.Add(new("Series", SelectedBook.SeriesIndex is null
                ? SelectedBook.Series!
                : $"{SelectedBook.Series} #{SelectedBook.SeriesIndex:0.##}"));
        }

        pairs.Add(new("Format", source?.Format.ToString().ToUpperInvariant() ?? SelectedBook.FormatText));

        var tags = source?.Tags.Count > 0
            ? string.Join(", ", source.Tags)
            : SelectedBook.Tags.Count > 0
                ? SelectedBook.TagsText
                : null;
        if (!string.IsNullOrWhiteSpace(tags))
        {
            pairs.Add(new("Genres", tags));
        }

        pairs.Add(new("Size", $"{SelectedBook.SizeBytes:N0} bytes"));
        return pairs;
    }

    private static string BuildCoverPlaceholderText(string title)
    {
        if (string.IsNullOrWhiteSpace(title))
        {
            return "BOOK";
        }

        var letter = title.Trim().FirstOrDefault(character => char.IsLetterOrDigit(character));
        return letter == default ? "BOOK" : char.ToUpperInvariant(letter).ToString();
    }

    private static string BuildAuthorsText(IReadOnlyList<string> authors) =>
        authors.Count == 0 ? "Unknown author" : string.Join(", ", authors);

    private static void ApplySelectionStyle(BookListItemModel item, bool isSelected)
    {
        item.CardBackgroundBrush = isSelected ? SelectedCardBackground : DefaultCardBackground;
        item.CardBorderBrush = isSelected ? SelectedCardBorder : DefaultCardBorder;
        item.CardBorderThickness = new Thickness(2);
    }

    private static IBrush CreateGradientBrush(long bookId, string title, string authorsText)
    {
        var palettes = new[]
        {
            (Start: Color.Parse("#6C4B3A"), End: Color.Parse("#D9A066")),
            (Start: Color.Parse("#2F4D5A"), End: Color.Parse("#8FB7C7")),
            (Start: Color.Parse("#5A2F3D"), End: Color.Parse("#C18E78")),
            (Start: Color.Parse("#40523B"), End: Color.Parse("#B6C58E")),
            (Start: Color.Parse("#314466"), End: Color.Parse("#D2B87A"))
        };

        var hash = HashCode.Combine(bookId, title, authorsText);
        var index = Math.Abs(hash) % palettes.Length;
        var palette = palettes[index];
        return new LinearGradientBrush
        {
            StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
            EndPoint = new RelativePoint(1, 1, RelativeUnit.Relative),
            GradientStops =
            [
                new GradientStop(palette.Start, 0),
                new GradientStop(palette.End, 1)
            ]
        };
    }
}

internal sealed record MetadataPair(string Label, string Value);

internal interface ICoverImageLoader
{
    IImage? Load(string absolutePath);
}

internal sealed class FileCoverImageLoader : ICoverImageLoader
{
    public IImage? Load(string absolutePath)
    {
        using var stream = File.OpenRead(absolutePath);
        return new Avalonia.Media.Imaging.Bitmap(stream);
    }
}
