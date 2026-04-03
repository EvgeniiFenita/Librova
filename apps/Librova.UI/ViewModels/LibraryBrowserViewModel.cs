using Avalonia;
using Avalonia.Media;
using Librova.UI.Desktop;
using Librova.UI.LibraryCatalog;
using Librova.UI.Mvvm;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class LibraryBrowserViewModel : ObservableObject
{
    private const double BytesPerMegabyte = 1024d * 1024d;
    private static readonly IBrush DefaultCardBackground = new SolidColorBrush(Color.Parse("#171D27"));
    private static readonly IBrush DefaultCardBorder = new SolidColorBrush(Color.Parse("#2B3647"));
    private static readonly IBrush SelectedCardBackground = new SolidColorBrush(Color.Parse("#1A2634"));
    private static readonly IBrush SelectedCardBorder = new SolidColorBrush(Color.Parse("#4CC2FF"));
    private static readonly IBrush RealCoverBackground = new SolidColorBrush(Color.Parse("#0F141C"));
    private readonly ILibraryCatalogService _libraryCatalogService;
    private readonly IPathSelectionService _pathSelectionService;
    private readonly ICoverImageLoader _coverImageLoader;
    private readonly string _libraryRoot;
    private readonly bool _hasConfiguredConverter;
    private CancellationTokenSource? _refreshDebounce;
    private CancellationTokenSource? _loadMoreCancellation;
    private bool _isLibraryStatisticsUnavailable;
    private string _searchText = string.Empty;
    private string _languageFilter = string.Empty;
    private string _statusText = "Library view is idle.";
    private bool _isBusy;
    private bool _isLoadingMore;
    private bool _isLoadingSelectionDetails;
    private int _pageSize = 120;
    private int _loadedPageCount = 0;
    private bool _hasMoreResults;
    private ulong _totalBookCount;
    private BookListItemModel? _selectedBook;
    private BookDetailsModel? _selectedBookDetails;
    private LibraryStatisticsModel _libraryStatistics = new();

    public LibraryBrowserViewModel(
        ILibraryCatalogService? libraryCatalogService = null,
        IPathSelectionService? pathSelectionService = null,
        string? libraryRoot = null,
        ICoverImageLoader? coverImageLoader = null,
        bool hasConfiguredConverter = false)
    {
        _libraryCatalogService = libraryCatalogService ?? new NullLibraryCatalogService();
        _pathSelectionService = pathSelectionService ?? new NullPathSelectionService();
        _coverImageLoader = coverImageLoader ?? new FileCoverImageLoader();
        _libraryRoot = libraryRoot ?? string.Empty;
        _hasConfiguredConverter = hasConfiguredConverter;
        RefreshCommand = new AsyncCommand(RefreshAsync, () => !IsBusy, HandleCommandErrorAsync);
        LoadDetailsCommand = new AsyncCommand(LoadSelectedBookDetailsAsync, () => HasSelectedBook && !IsLoadingSelectionDetails, HandleCommandErrorAsync);
        ExportSelectedBookCommand = new AsyncCommand(ExportSelectedBookAsync, () => !IsBusy && HasSelectedBook, HandleCommandErrorAsync);
        ExportSelectedBookAsEpubCommand = new AsyncCommand(ExportSelectedBookAsEpubAsync, () => CanExportSelectedBookAsEpub, HandleCommandErrorAsync);
        MoveSelectedBookToTrashCommand = new AsyncCommand(MoveSelectedBookToTrashAsync, () => !IsBusy && HasSelectedBook, HandleCommandErrorAsync);
        SelectBookCommand = new AsyncCommand<BookListItemModel?>(ToggleSelectedBookAsync, book => book is not null);
        CloseSelectionCommand = new AsyncCommand(CloseSelectionAsync, () => HasSelectedBook, HandleCommandErrorAsync);
        AvailableLanguageFilters.Add("All languages");
    }

    public ObservableCollection<BookListItemModel> Books { get; } = [];
    public ObservableCollection<string> AvailableLanguageFilters { get; } = [];
    public AsyncCommand RefreshCommand { get; }
    public AsyncCommand LoadDetailsCommand { get; }
    public AsyncCommand ExportSelectedBookCommand { get; }
    public AsyncCommand ExportSelectedBookAsEpubCommand { get; }
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
            if (value is null)
            {
                RaisePropertyChanged(nameof(SelectedLanguageFilter));
                return;
            }

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
            SetProperty(ref _pageSize, effectiveValue);
        }
    }

    public bool HasMoreResults
    {
        get => _hasMoreResults;
        private set
        {
            if (SetProperty(ref _hasMoreResults, value))
            {
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
                RaisePropertyChanged(nameof(ShowExportAsEpubAction));
                RaisePropertyChanged(nameof(ShowExportAsEpubHint));
                RaisePropertyChanged(nameof(ExportAsEpubHintText));
                RaisePropertyChanged(nameof(CanExportSelectedBookAsEpub));
                LoadDetailsCommand.RaiseCanExecuteChanged();
                ExportSelectedBookCommand.RaiseCanExecuteChanged();
                ExportSelectedBookAsEpubCommand.RaiseCanExecuteChanged();
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
                RaisePropertyChanged(nameof(ShowExportAsEpubAction));
                RaisePropertyChanged(nameof(ShowExportAsEpubHint));
                RaisePropertyChanged(nameof(ExportAsEpubHintText));
                RaisePropertyChanged(nameof(CanExportSelectedBookAsEpub));
                ExportSelectedBookAsEpubCommand.RaiseCanExecuteChanged();
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
                LoadDetailsCommand.RaiseCanExecuteChanged();
                ExportSelectedBookCommand.RaiseCanExecuteChanged();
                ExportSelectedBookAsEpubCommand.RaiseCanExecuteChanged();
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

    public bool IsLoadingMore
    {
        get => _isLoadingMore;
        private set
        {
            if (SetProperty(ref _isLoadingMore, value))
            {
                RaisePropertyChanged(nameof(ShowLoadMoreIndicator));
            }
        }
    }

    public bool HasBooks => Books.Count > 0;
    public bool HasSelectedBook => SelectedBook is not null;
    public bool ShowDetailsPanel => HasSelectedBook;
    public bool ShowDetailsPanelLoading => HasSelectedBook && IsLoadingSelectionDetails;
    public bool ShowExportAsEpubAction => HasSelectedBook && SelectedBookFormat is BookFormatModel.Fb2 && _hasConfiguredConverter;
    public bool ShowExportAsEpubHint => HasSelectedBook && SelectedBookFormat is BookFormatModel.Fb2 && !_hasConfiguredConverter;
    public string ExportAsEpubHintText => "Configure a converter in Settings to enable EPUB export.";
    public bool CanExportSelectedBookAsEpub => !IsBusy && ShowExportAsEpubAction;
    public bool ShowEmptyState => !HasBooks;
    public bool ShowLoadMoreIndicator => HasBooks && IsLoadingMore;
    public string BookCountText => FormatBookCount(_totalBookCount);
    public string LibraryStatisticsText =>
        _isLibraryStatisticsUnavailable
            ? "Library summary unavailable."
            : $"Library: {FormatBookCount(_libraryStatistics.BookCount)}, {FormatSizeInMegabytes(_libraryStatistics.TotalManagedBookSizeBytes)}";
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
        await RefreshBatchAsync(1);
    }

    public async Task LoadMoreAsync()
    {
        if (IsBusy || IsLoadingMore || !HasMoreResults)
        {
            return;
        }

        try
        {
            await AppendNextBatchAsync();
        }
        catch (Exception error)
        {
            await HandleCommandErrorAsync(error);
        }
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
        await ExportSelectedBookCoreAsync(null);
    }

    public async Task ExportSelectedBookAsEpubAsync()
    {
        await ExportSelectedBookCoreAsync(BookFormatModel.Epub);
    }

    private async Task ExportSelectedBookCoreAsync(BookFormatModel? exportFormat)
    {
        if (SelectedBook is null)
        {
            return;
        }

        var suggestedFileName = BuildSuggestedExportFileName(exportFormat);
        var destinationPath = await _pathSelectionService.PickExportDestinationAsync(suggestedFileName, default);
        if (string.IsNullOrWhiteSpace(destinationPath))
        {
            StatusText = "Export was cancelled before a destination was chosen.";
            return;
        }

        IsBusy = true;
        StatusText = exportFormat is BookFormatModel.Epub
            ? $"Exporting '{SelectedBook.Title}' as EPUB..."
            : $"Exporting '{SelectedBook.Title}'...";

        try
        {
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(10));
            var exportedPath = await _libraryCatalogService.ExportBookAsync(
                SelectedBook.BookId,
                destinationPath,
                exportFormat,
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            StatusText = string.IsNullOrWhiteSpace(exportedPath)
                ? "Selected book could not be exported."
                : exportFormat is BookFormatModel.Epub
                    ? $"Exported '{SelectedBook.Title}' as EPUB to '{exportedPath}'."
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
            var loadedPageCount = Math.Max(_loadedPageCount, 1);
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

            await RefreshLoadedRangeAsync(loadedPageCount);

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
            await RefreshBatchAsync(1);
        }
        catch (OperationCanceledException)
        {
        }
        catch (Exception error)
        {
            await HandleCommandErrorAsync(error);
        }
    }

    private async Task RefreshBatchAsync(int batchNumber)
    {
        IsBusy = true;
        StatusText = "Refreshing library...";
        var previousSelectionId = SelectedBook?.BookId;

        try
        {
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(10));
            var page = await _libraryCatalogService.ListBooksAsync(
                BuildRequest(batchNumber),
                TimeSpan.FromSeconds(5),
                cancellation.Token);
            var libraryStatisticsUnavailable = false;

            try
            {
                _libraryStatistics = await _libraryCatalogService.GetLibraryStatisticsAsync(
                    TimeSpan.FromSeconds(5),
                    cancellation.Token);
            }
            catch (Exception)
            {
                libraryStatisticsUnavailable = true;
            }

            var visibleItems = page.Items.Select(Prepare).ToArray();
            Books.Clear();
            foreach (var item in visibleItems)
            {
                Books.Add(item);
            }

            UpdateAvailableLanguages(page.AvailableLanguages);
            _loadedPageCount = batchNumber;
            _totalBookCount = page.TotalCount;

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

            _isLibraryStatisticsUnavailable = libraryStatisticsUnavailable;
            HasMoreResults = (ulong)Books.Count < _totalBookCount;
            RaisePropertyChanged(nameof(HasBooks));
            RaisePropertyChanged(nameof(ShowEmptyState));
            RaisePropertyChanged(nameof(BookCountText));
            RaisePropertyChanged(nameof(LibraryStatisticsText));
            RaisePropertyChanged(nameof(EmptyStateTitle));
            RaisePropertyChanged(nameof(EmptyStateDescription));
            StatusText = BuildRefreshStatusText(Books.Count, _totalBookCount, _isLibraryStatisticsUnavailable);
        }
        finally
        {
            IsBusy = false;
        }
    }

    private async Task RefreshLoadedRangeAsync(int loadedPageCount)
    {
        await RefreshRangeAsync(Math.Max(1, loadedPageCount) * PageSize);
    }

    private async Task AppendNextBatchAsync()
    {
        if (_loadedPageCount <= 0)
        {
            await RefreshBatchAsync(1);
            return;
        }

        _loadMoreCancellation?.Cancel();
        _loadMoreCancellation?.Dispose();
        _loadMoreCancellation = new CancellationTokenSource(TimeSpan.FromSeconds(10));

        IsLoadingMore = true;
        StatusText = "Loading more books...";

        try
        {
            var nextBatchNumber = _loadedPageCount + 1;
            var page = await _libraryCatalogService.ListBooksAsync(
                BuildRequest(nextBatchNumber),
                TimeSpan.FromSeconds(5),
                _loadMoreCancellation.Token);

            foreach (var item in page.Items.Select(Prepare))
            {
                if (Books.Any(existing => existing.BookId == item.BookId))
                {
                    continue;
                }

                Books.Add(item);
            }

            _totalBookCount = page.TotalCount;
            _loadedPageCount = nextBatchNumber;
            HasMoreResults = (ulong)Books.Count < _totalBookCount;
            UpdateAvailableLanguages(page.AvailableLanguages);
            RaisePropertyChanged(nameof(HasBooks));
            RaisePropertyChanged(nameof(ShowEmptyState));
            RaisePropertyChanged(nameof(BookCountText));
            RaisePropertyChanged(nameof(EmptyStateTitle));
            RaisePropertyChanged(nameof(EmptyStateDescription));
            StatusText = BuildRefreshStatusText(Books.Count, _totalBookCount, _isLibraryStatisticsUnavailable);
        }
        finally
        {
            IsLoadingMore = false;
        }
    }

    private Task HandleCommandErrorAsync(Exception error)
    {
        StatusText = string.IsNullOrWhiteSpace(error.Message) ? "Failed to update the library view." : error.Message;
        return Task.CompletedTask;
    }

    private async Task RefreshRangeAsync(int itemLimit)
    {
        IsBusy = true;
        StatusText = "Refreshing library...";
        var previousSelectionId = SelectedBook?.BookId;

        try
        {
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(10));
            var page = await _libraryCatalogService.ListBooksAsync(
                BuildInitialRangeRequest(itemLimit),
                TimeSpan.FromSeconds(5),
                cancellation.Token);
            var libraryStatisticsUnavailable = false;

            try
            {
                _libraryStatistics = await _libraryCatalogService.GetLibraryStatisticsAsync(
                    TimeSpan.FromSeconds(5),
                    cancellation.Token);
            }
            catch (Exception)
            {
                libraryStatisticsUnavailable = true;
            }

            var visibleItems = page.Items.Select(Prepare).ToArray();
            Books.Clear();
            foreach (var item in visibleItems)
            {
                Books.Add(item);
            }

            UpdateAvailableLanguages(page.AvailableLanguages);
            _loadedPageCount = Math.Max(1, (visibleItems.Length + PageSize - 1) / PageSize);
            _totalBookCount = page.TotalCount;

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

            _isLibraryStatisticsUnavailable = libraryStatisticsUnavailable;
            HasMoreResults = (ulong)Books.Count < _totalBookCount;
            RaisePropertyChanged(nameof(HasBooks));
            RaisePropertyChanged(nameof(ShowEmptyState));
            RaisePropertyChanged(nameof(BookCountText));
            RaisePropertyChanged(nameof(LibraryStatisticsText));
            RaisePropertyChanged(nameof(EmptyStateTitle));
            RaisePropertyChanged(nameof(EmptyStateDescription));
            StatusText = BuildRefreshStatusText(Books.Count, _totalBookCount, _isLibraryStatisticsUnavailable);
        }
        finally
        {
            IsBusy = false;
        }
    }

    private BookListRequestModel BuildRequest(int batchNumber) =>
        new()
        {
            Text = SearchText,
            Language = string.IsNullOrWhiteSpace(LanguageFilter) ? null : LanguageFilter,
            Offset = checked((ulong)Math.Max(0, batchNumber - 1) * (ulong)PageSize),
            Limit = (ulong)PageSize
        };

    private BookListRequestModel BuildInitialRangeRequest(int itemLimit) =>
        new()
        {
            Text = SearchText,
            Language = string.IsNullOrWhiteSpace(LanguageFilter) ? null : LanguageFilter,
            Offset = 0,
            Limit = (ulong)Math.Max(1, itemLimit)
        };

    private BookListItemModel Prepare(BookListItemModel item)
    {
        item.ResolvedCoverImage = LoadCoverImage(item.CoverPath);
        item.CoverBackgroundBrush = CreateCoverBackgroundBrush(item.ResolvedCoverImage, item.BookId, item.Title, item.AuthorsText);
        item.CoverPlaceholderText = BuildCoverPlaceholderText(item.Title);
        item.IsSelected = false;
        ApplySelectionStyle(item, isSelected: false);
        return item;
    }

    private BookDetailsModel Prepare(BookDetailsModel item)
    {
        item.ResolvedCoverImage = LoadCoverImage(item.CoverPath);
        item.CoverBackgroundBrush = CreateCoverBackgroundBrush(
            item.ResolvedCoverImage,
            item.BookId,
            item.Title,
            BuildAuthorsText(item.Authors));
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

    private void UpdateAvailableLanguages(IEnumerable<string> availableLanguages)
    {
        var items = new List<string> { "All languages" };
        items.AddRange(availableLanguages
            .Where(language => !string.IsNullOrWhiteSpace(language))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(language => language, StringComparer.OrdinalIgnoreCase));

        if (!string.IsNullOrWhiteSpace(LanguageFilter) && !items.Contains(LanguageFilter, StringComparer.OrdinalIgnoreCase))
        {
            items.Add(LanguageFilter);
        }

        SynchronizeLanguageFilters(items.Distinct(StringComparer.OrdinalIgnoreCase).ToArray());

        RaisePropertyChanged(nameof(SelectedLanguageFilter));
    }

    private void SynchronizeLanguageFilters(IReadOnlyList<string> desiredItems)
    {
        for (var index = AvailableLanguageFilters.Count - 1; index >= 0; index--)
        {
            if (!desiredItems.Contains(AvailableLanguageFilters[index], StringComparer.OrdinalIgnoreCase))
            {
                AvailableLanguageFilters.RemoveAt(index);
            }
        }

        for (var desiredIndex = 0; desiredIndex < desiredItems.Count; desiredIndex++)
        {
            var desiredItem = desiredItems[desiredIndex];
            var existingIndex = IndexOfLanguageFilter(desiredItem);
            if (existingIndex < 0)
            {
                AvailableLanguageFilters.Insert(desiredIndex, desiredItem);
                continue;
            }

            if (existingIndex != desiredIndex)
            {
                AvailableLanguageFilters.Move(existingIndex, desiredIndex);
            }
        }
    }

    private int IndexOfLanguageFilter(string value)
    {
        for (var index = 0; index < AvailableLanguageFilters.Count; index++)
        {
            if (string.Equals(AvailableLanguageFilters[index], value, StringComparison.OrdinalIgnoreCase))
            {
                return index;
            }
        }

        return -1;
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

        pairs.Add(new("Size", FormatSizeInMegabytes(SelectedBook.SizeBytes)));
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

    private static string FormatBookCount(ulong bookCount) =>
        bookCount == 1 ? "1 book" : $"{bookCount:N0} books";

    private static string FormatSizeInMegabytes(ulong sizeBytes)
    {
        var megabytes = sizeBytes / BytesPerMegabyte;
        return string.Create(
            CultureInfo.InvariantCulture,
            $"{megabytes:F2} MB");
    }

    private static string BuildRefreshStatusText(int visibleBookCount, ulong totalBookCount, bool libraryStatisticsUnavailable)
    {
        var baseText = visibleBookCount == 0
            ? "No books found for the current filter."
            : (ulong)visibleBookCount >= totalBookCount
                ? $"Loaded {visibleBookCount} book(s)."
                : $"Loaded {visibleBookCount} of {totalBookCount} book(s).";
        return libraryStatisticsUnavailable
            ? baseText + " Library summary unavailable."
            : baseText;
    }

    private BookFormatModel? SelectedBookFormat => SelectedBookDetails?.Format ?? SelectedBook?.Format;

    private string BuildSuggestedExportFileName(BookFormatModel? exportFormat)
    {
        const string fallbackBaseName = "book";

        var title = SanitizeFileNamePart(SelectedBookDetails?.Title ?? SelectedBook?.Title);
        var authors = SanitizeFileNamePart(
            SelectedBookDetails is not null
                ? BuildAuthorsText(SelectedBookDetails.Authors)
                : SelectedBook?.AuthorsText);

        var baseName = !string.IsNullOrWhiteSpace(title) && !string.IsNullOrWhiteSpace(authors)
            ? $"{title} - {authors}"
            : !string.IsNullOrWhiteSpace(title)
                ? title
                : !string.IsNullOrWhiteSpace(authors)
                    ? authors
                    : fallbackBaseName;

        var extension = exportFormat switch
        {
            BookFormatModel.Fb2 => ".fb2",
            BookFormatModel.Epub => ".epub",
            _ => Path.GetExtension(SelectedBook?.ManagedPath)
        };
        if (string.IsNullOrWhiteSpace(extension))
        {
            extension = (SelectedBookDetails?.Format ?? SelectedBook?.Format ?? BookFormatModel.Epub) switch
            {
                BookFormatModel.Fb2 => ".fb2",
                _ => ".epub"
            };
        }

        return baseName + extension.ToLowerInvariant();
    }

    private static string SanitizeFileNamePart(string? value)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            return string.Empty;
        }

        var builder = new StringBuilder(value.Length);
        var invalidCharacters = Path.GetInvalidFileNameChars();
        var previousWasWhitespace = false;

        foreach (var character in value.Trim())
        {
            if (Array.IndexOf(invalidCharacters, character) >= 0 || char.IsControl(character))
            {
                if (!previousWasWhitespace && builder.Length > 0)
                {
                    builder.Append(' ');
                    previousWasWhitespace = true;
                }

                continue;
            }

            if (char.IsWhiteSpace(character))
            {
                if (!previousWasWhitespace && builder.Length > 0)
                {
                    builder.Append(' ');
                    previousWasWhitespace = true;
                }

                continue;
            }

            builder.Append(character);
            previousWasWhitespace = false;
        }

        return builder.ToString().Trim(' ', '.');
    }

    private static void ApplySelectionStyle(BookListItemModel item, bool isSelected)
    {
        item.CardBackgroundBrush = isSelected ? SelectedCardBackground : DefaultCardBackground;
        item.CardBorderBrush = isSelected ? SelectedCardBorder : DefaultCardBorder;
        item.CardBorderThickness = isSelected ? new Thickness(2) : new Thickness(0);
    }

    private static IBrush CreateCoverBackgroundBrush(IImage? resolvedCoverImage, long bookId, string title, string authorsText) =>
        resolvedCoverImage is null
            ? CreateGradientBrush(bookId, title, authorsText)
            : RealCoverBackground;

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
