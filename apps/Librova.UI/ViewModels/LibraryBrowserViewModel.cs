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
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class LibraryBrowserViewModel : ObservableObject, IDisposable
{
    private const double BytesPerMegabyte = 1024d * 1024d;
    private static readonly IBrush DefaultCardBackground = new SolidColorBrush(Color.Parse("#1C160C"));
    private static readonly IBrush DefaultCardBorder = new SolidColorBrush(Color.Parse("#2A200E"));
    private static readonly IBrush SelectedCardBackground = new SolidColorBrush(Color.Parse("#221A0E"));
    private static readonly IBrush SelectedCardBorder = new SolidColorBrush(Color.Parse("#F5A623"));
    private readonly ILibraryCatalogService _libraryCatalogService;
    private readonly IPathSelectionService _pathSelectionService;
    private readonly LibraryCoverPresentationService _coverPresentationService;
    private readonly bool _hasConfiguredConverter;
    private readonly CancellationTokenSource _lifetimeCancellation = new();
    private CancellationTokenSource? _refreshDebounce;
    private CancellationTokenSource? _loadMoreCancellation;
    private bool _isDisposed;
    private bool _isLibraryStatisticsUnavailable;
    private string _searchText = string.Empty;
    private string _languageFilter = string.Empty;
    private SortKeyOption _selectedSortKey = SortKeyOption.Default;
    private bool _sortDescending;
    private string _statusText = "Library view is idle.";
    private bool _isBusy;
    private bool _isExportBusy;
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
        bool hasConfiguredConverter = false,
        BookSortModel? initialSortKey = null,
        bool initialSortDescending = false)
    {
        _libraryCatalogService = libraryCatalogService ?? new NullLibraryCatalogService();
        _pathSelectionService = pathSelectionService ?? new NullPathSelectionService();
        _coverPresentationService = new LibraryCoverPresentationService(libraryRoot, coverImageLoader);
        _hasConfiguredConverter = hasConfiguredConverter;
        _selectedSortKey = SortKeyOption.For(initialSortKey ?? BookSortModel.Title);
        _sortDescending = initialSortDescending;
        RefreshCommand = new AsyncCommand(RefreshAsync, () => !IsBusy, HandleCommandErrorAsync);
        LoadDetailsCommand = new AsyncCommand(LoadSelectedBookDetailsAsync, () => HasSelectedBook && !IsLoadingSelectionDetails, HandleCommandErrorAsync);
        ExportSelectedBookCommand = new AsyncCommand(ExportSelectedBookAsync, () => !IsBusy && HasSelectedBook, HandleCommandErrorAsync);
        ExportSelectedBookAsEpubCommand = new AsyncCommand(ExportSelectedBookAsEpubAsync, () => CanExportSelectedBookAsEpub, HandleCommandErrorAsync);
        MoveSelectedBookToTrashCommand = new AsyncCommand(MoveSelectedBookToTrashAsync, () => !IsBusy && HasSelectedBook, HandleCommandErrorAsync);
        SelectBookCommand = new AsyncCommand<BookListItemModel?>(ToggleSelectedBookAsync, book => book is not null);
        CloseSelectionCommand = new AsyncCommand(CloseSelectionAsync, () => HasSelectedBook, HandleCommandErrorAsync);
        ToggleSortDirectionCommand = new AsyncCommand(ToggleSortDirectionAsync, () => true, HandleCommandErrorAsync);
        AvailableLanguageFilters.Add("All languages");
    }

    public ObservableCollection<BookListItemModel> Books { get; } = [];
    public ObservableCollection<string> AvailableLanguageFilters { get; } = [];
    public IReadOnlyList<SortKeyOption> AvailableSortKeys { get; } = SortKeyOption.All;
    public AsyncCommand RefreshCommand { get; }
    public AsyncCommand LoadDetailsCommand { get; }
    public AsyncCommand ExportSelectedBookCommand { get; }
    public AsyncCommand ExportSelectedBookAsEpubCommand { get; }
    public AsyncCommand MoveSelectedBookToTrashCommand { get; }
    public AsyncCommand<BookListItemModel?> SelectBookCommand { get; }
    public AsyncCommand CloseSelectionCommand { get; }
    public AsyncCommand ToggleSortDirectionCommand { get; }

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

    public SortKeyOption? SelectedSortKey
    {
        get => _selectedSortKey;
        set
        {
            if (value is null)
            {
                RaisePropertyChanged(nameof(SelectedSortKey));
                return;
            }

            if (_selectedSortKey == value)
            {
                return;
            }

            _selectedSortKey = value;
            RaisePropertyChanged(nameof(SelectedSortKey));
            ScheduleRefresh();
        }
    }

    public bool SortDescending
    {
        get => _sortDescending;
        private set
        {
            if (SetProperty(ref _sortDescending, value))
            {
                ScheduleRefresh();
            }
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

    public bool IsExportBusy
    {
        get => _isExportBusy;
        private set => SetProperty(ref _isExportBusy, value);
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
            : $"Library: {FormatBookCount(_libraryStatistics.BookCount)}, {FormatSizeInMegabytes(_libraryStatistics.TotalLibrarySizeBytes)}";
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
        if (_isDisposed)
        {
            return;
        }

        try
        {
            await RefreshBatchAsync(1);
        }
        catch (OperationCanceledException) when (IsLifetimeCancellationRequested())
        {
        }
    }

    public async Task LoadMoreAsync()
    {
        if (_isDisposed || IsBusy || IsLoadingMore || !HasMoreResults)
        {
            return;
        }

        try
        {
            await AppendNextBatchAsync();
        }
        catch (OperationCanceledException) when (IsLifetimeCancellationRequested())
        {
        }
        catch (Exception error)
        {
            await HandleCommandErrorAsync(error);
        }
    }

    public async Task ToggleSelectedBookAsync(BookListItemModel? book)
    {
        if (_isDisposed || book is null)
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
        if (_isDisposed)
        {
            return Task.CompletedTask;
        }

        SelectedBook = null;
        StatusText = "Closed book details.";
        return Task.CompletedTask;
    }

    public async Task LoadSelectedBookDetailsAsync()
    {
        if (_isDisposed || SelectedBook is null)
        {
            return;
        }

        IsLoadingSelectionDetails = true;
        StatusText = $"Loading details for '{SelectedBook.Title}'...";

        try
        {
            using var cancellation = CreateOperationCancellationSource(TimeSpan.FromSeconds(10));
            var details = await _libraryCatalogService.GetBookDetailsAsync(
                SelectedBook.BookId,
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            if (IsLifetimeCancellationRequested(cancellation.Token))
            {
                return;
            }

            SelectedBookDetails = details is null ? null : Prepare(details);
            StatusText = SelectedBookDetails is null
                ? "Book details were not found."
                : $"Loaded details for '{SelectedBookDetails.Title}'.";
        }
        catch (OperationCanceledException) when (IsLifetimeCancellationRequested())
        {
        }
        finally
        {
            if (!_isDisposed)
            {
                IsLoadingSelectionDetails = false;
            }
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
        if (_isDisposed || SelectedBook is null)
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
        IsExportBusy = true;
        StatusText = exportFormat is BookFormatModel.Epub
            ? $"Exporting '{TruncateTitle(SelectedBook.Title)}' as EPUB..."
            : $"Exporting '{TruncateTitle(SelectedBook.Title)}'...";

        // EPUB export may require decompressing a managed FB2 and running an external converter,
        // which can take tens of seconds. Use a generous timeout for that path.
        var operationTimeout = exportFormat is BookFormatModel.Epub
            ? TimeSpan.FromMinutes(3)
            : TimeSpan.FromSeconds(30);
        var transportTimeout = exportFormat is BookFormatModel.Epub
            ? TimeSpan.FromMinutes(2)
            : TimeSpan.FromSeconds(10);

        try
        {
            using var cancellation = CreateOperationCancellationSource(operationTimeout);
            var exportedPath = await _libraryCatalogService.ExportBookAsync(
                SelectedBook.BookId,
                destinationPath,
                exportFormat,
                transportTimeout,
                cancellation.Token);

            if (IsLifetimeCancellationRequested(cancellation.Token))
            {
                return;
            }

            StatusText = string.IsNullOrWhiteSpace(exportedPath)
                ? "Selected book could not be exported."
                : exportFormat is BookFormatModel.Epub
                    ? $"Exported '{SelectedBook.Title}' as EPUB to '{exportedPath}'."
                    : $"Exported '{SelectedBook.Title}' to '{exportedPath}'.";
        }
        catch (OperationCanceledException) when (IsLifetimeCancellationRequested())
        {
        }
        finally
        {
            if (!_isDisposed)
            {
                IsExportBusy = false;
                IsBusy = false;
            }
        }
    }

    public async Task MoveSelectedBookToTrashAsync()
    {
        if (_isDisposed || SelectedBook is null)
        {
            return;
        }

        IsBusy = true;
        StatusText = $"Moving '{SelectedBook.Title}' to Recycle Bin...";

        try
        {
            var deletedTitle = SelectedBook.Title;
            var loadedPageCount = Math.Max(_loadedPageCount, 1);
            using var cancellation = CreateOperationCancellationSource(TimeSpan.FromSeconds(10));
            var deleteResult = await _libraryCatalogService.MoveBookToTrashAsync(
                SelectedBook.BookId,
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            if (IsLifetimeCancellationRequested(cancellation.Token))
            {
                return;
            }

            if (deleteResult is null)
            {
                StatusText = "Selected book could not be moved to Recycle Bin.";
                return;
            }

            await RefreshLoadedRangeAsync(loadedPageCount);

            StatusText = GetDeleteStatusText(deletedTitle, deleteResult);
        }
        catch (OperationCanceledException) when (IsLifetimeCancellationRequested())
        {
        }
        finally
        {
            if (!_isDisposed)
            {
                IsBusy = false;
            }
        }
    }

    private static string GetDeleteStatusText(string deletedTitle, DeleteBookResultModel deleteResult)
    {
        if (deleteResult.HasOrphanedFiles)
        {
            return deleteResult.Destination == DeleteDestinationModel.RecycleBin
                ? $"Removed '{deletedTitle}' from the library, moved the available managed files to Recycle Bin, and left some managed files on disk."
                : $"Removed '{deletedTitle}' from the library, but some managed files could not be moved and were left on disk.";
        }

        return deleteResult.Destination == DeleteDestinationModel.RecycleBin
            ? $"Moved '{deletedTitle}' to Recycle Bin."
            : $"Moved '{deletedTitle}' to library Trash because Windows Recycle Bin was unavailable.";
    }

    private void ScheduleRefresh()
    {
        if (_isDisposed)
        {
            return;
        }

        CancelAndDispose(ref _refreshDebounce);
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
        catch (OperationCanceledException) when (IsLifetimeCancellationRequested(cancellationToken))
        {
        }
        catch (Exception error)
        {
            await HandleCommandErrorAsync(error);
        }
    }

    private async Task RefreshBatchAsync(int batchNumber)
    {
        if (_isDisposed)
        {
            return;
        }

        IsBusy = true;
        StatusText = "Refreshing library...";
        var previousSelectionId = SelectedBook?.BookId;

        try
        {
            using var cancellation = CreateOperationCancellationSource(TimeSpan.FromSeconds(10));
            var page = await _libraryCatalogService.ListBooksAsync(
                BuildRequest(batchNumber),
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            if (IsLifetimeCancellationRequested(cancellation.Token))
            {
                return;
            }

            var libraryStatisticsUnavailable = false;

            try
            {
                _libraryStatistics = await _libraryCatalogService.GetLibraryStatisticsAsync(
                    TimeSpan.FromSeconds(5),
                    cancellation.Token);
            }
            catch (OperationCanceledException) when (IsLifetimeCancellationRequested(cancellation.Token))
            {
                return;
            }
            catch (Exception)
            {
                libraryStatisticsUnavailable = true;
            }

            if (IsLifetimeCancellationRequested(cancellation.Token))
            {
                return;
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
            if (!_isDisposed)
            {
                IsBusy = false;
            }
        }
    }

    private async Task RefreshLoadedRangeAsync(int loadedPageCount)
    {
        await RefreshRangeAsync(Math.Max(1, loadedPageCount) * PageSize);
    }

    private async Task AppendNextBatchAsync()
    {
        if (_isDisposed)
        {
            return;
        }

        if (_loadedPageCount <= 0)
        {
            await RefreshBatchAsync(1);
            return;
        }

        CancelAndDispose(ref _loadMoreCancellation);
        _loadMoreCancellation = CancellationTokenSource.CreateLinkedTokenSource(_lifetimeCancellation.Token);
        _loadMoreCancellation.CancelAfter(TimeSpan.FromSeconds(10));

        IsLoadingMore = true;
        StatusText = "Loading more books...";

        try
        {
            var nextBatchNumber = _loadedPageCount + 1;
            var page = await _libraryCatalogService.ListBooksAsync(
                BuildRequest(nextBatchNumber),
                TimeSpan.FromSeconds(5),
                _loadMoreCancellation.Token);

            if (IsLifetimeCancellationRequested(_loadMoreCancellation.Token))
            {
                return;
            }

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
            if (!_isDisposed)
            {
                IsLoadingMore = false;
            }
        }
    }

    private Task HandleCommandErrorAsync(Exception error)
    {
        StatusText = string.IsNullOrWhiteSpace(error.Message) ? "Failed to update the library view." : error.Message;
        return Task.CompletedTask;
    }

    private async Task RefreshRangeAsync(int itemLimit)
    {
        if (_isDisposed)
        {
            return;
        }

        IsBusy = true;
        StatusText = "Refreshing library...";
        var previousSelectionId = SelectedBook?.BookId;

        try
        {
            using var cancellation = CreateOperationCancellationSource(TimeSpan.FromSeconds(10));
            var page = await _libraryCatalogService.ListBooksAsync(
                BuildInitialRangeRequest(itemLimit),
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            if (IsLifetimeCancellationRequested(cancellation.Token))
            {
                return;
            }

            var libraryStatisticsUnavailable = false;

            try
            {
                _libraryStatistics = await _libraryCatalogService.GetLibraryStatisticsAsync(
                    TimeSpan.FromSeconds(5),
                    cancellation.Token);
            }
            catch (OperationCanceledException) when (IsLifetimeCancellationRequested(cancellation.Token))
            {
                return;
            }
            catch (Exception)
            {
                libraryStatisticsUnavailable = true;
            }

            if (IsLifetimeCancellationRequested(cancellation.Token))
            {
                return;
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
            if (!_isDisposed)
            {
                IsBusy = false;
            }
        }
    }

    public void Dispose()
    {
        if (_isDisposed)
        {
            return;
        }

        _isDisposed = true;
        CancelAndDispose(ref _refreshDebounce);
        CancelAndDispose(ref _loadMoreCancellation);
        _lifetimeCancellation.Cancel();
        _lifetimeCancellation.Dispose();
    }

    private CancellationTokenSource CreateOperationCancellationSource(TimeSpan timeout)
    {
        var cancellation = CancellationTokenSource.CreateLinkedTokenSource(_lifetimeCancellation.Token);
        cancellation.CancelAfter(timeout);
        return cancellation;
    }

    private bool IsLifetimeCancellationRequested(CancellationToken cancellationToken = default) =>
        _isDisposed
        || _lifetimeCancellation.IsCancellationRequested
        || cancellationToken.IsCancellationRequested;

    private static void CancelAndDispose(ref CancellationTokenSource? cancellation)
    {
        if (cancellation is null)
        {
            return;
        }

        cancellation.Cancel();
        cancellation.Dispose();
        cancellation = null;
    }

    private BookListRequestModel BuildRequest(int batchNumber) =>
        new()
        {
            Text = SearchText,
            Language = string.IsNullOrWhiteSpace(LanguageFilter) ? null : LanguageFilter,
            SortBy = _selectedSortKey.Key,
            SortDirection = _sortDescending ? BookSortDirectionModel.Descending : BookSortDirectionModel.Ascending,
            Offset = checked((ulong)Math.Max(0, batchNumber - 1) * (ulong)PageSize),
            Limit = (ulong)PageSize
        };

    private BookListRequestModel BuildInitialRangeRequest(int itemLimit) =>
        new()
        {
            Text = SearchText,
            Language = string.IsNullOrWhiteSpace(LanguageFilter) ? null : LanguageFilter,
            SortBy = _selectedSortKey.Key,
            SortDirection = _sortDescending ? BookSortDirectionModel.Descending : BookSortDirectionModel.Ascending,
            Offset = 0,
            Limit = (ulong)Math.Max(1, itemLimit)
        };

    private Task ToggleSortDirectionAsync()
    {
        SortDescending = !SortDescending;
        return Task.CompletedTask;
    }

    private BookListItemModel Prepare(BookListItemModel item)
    {
        _coverPresentationService.Prepare(item);
        item.IsSelected = false;
        ApplySelectionStyle(item, isSelected: false);
        return item;
    }

    private BookDetailsModel Prepare(BookDetailsModel item)
    {
        return _coverPresentationService.Prepare(item);
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

    private static string BuildAuthorsText(IReadOnlyList<string> authors) =>
        authors.Count == 0 ? "Unknown author" : string.Join(", ", authors);

    private static string FormatBookCount(ulong bookCount) =>
        bookCount == 1 ? "1 book" : $"{bookCount:N0} books";

    private static string TruncateTitle(string? title, int maxLength = 40)
    {
        if (string.IsNullOrEmpty(title) || title.Length <= maxLength)
            return title ?? string.Empty;
        return string.Concat(title.AsSpan(0, maxLength), "…");
    }

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
            _ => (SelectedBookDetails?.Format ?? SelectedBook?.Format) switch
            {
                BookFormatModel.Fb2 => ".fb2",
                BookFormatModel.Epub => ".epub",
                _ => null
            }
        };
        if (string.IsNullOrWhiteSpace(extension))
        {
            extension = ".epub";
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
}

internal sealed record MetadataPair(string Label, string Value);
