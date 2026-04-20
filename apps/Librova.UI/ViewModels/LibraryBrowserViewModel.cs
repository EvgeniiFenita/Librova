using Avalonia;
using Avalonia.Media;
using Librova.UI.Desktop;
using Librova.UI.LibraryCatalog;
using Librova.UI.Logging;
using Librova.UI.Mvvm;
using Librova.UI.Styling;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Globalization;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class LibraryBrowserViewModel : ObservableObject, IDisposable
{
    private const double BytesPerMegabyte = 1024d * 1024d;
    private static readonly IBrush DefaultCardBackground = UiThemeResources.AppSurfaceMutedBrush;
    private static readonly IBrush DefaultCardBorder = UiThemeResources.AppBorderBrush;
    private static readonly IBrush SelectedCardBackground = UiThemeResources.AppSurfaceAltBrush;
    private static readonly IBrush SelectedCardBorder = UiThemeResources.AppAccentBrush;
    private readonly ILibraryCatalogService _libraryCatalogService;
    private readonly IPathSelectionService _pathSelectionService;
    private readonly LibraryCoverPresentationService _coverPresentationService;
    private readonly bool _hasConfiguredConverter;
    private readonly LibraryBrowseQueryState _browseState;
    private readonly LibraryBrowseRefreshController _refreshController;
    private readonly LibrarySelectionWorkflowController _selectionWorkflowController;
    private readonly CancellationTokenSource _lifetimeCancellation = new();
    private CancellationTokenSource? _refreshDebounce;
    private CancellationTokenSource? _loadMoreCancellation;
    private bool _isDisposed;
    private bool _isLibraryStatisticsUnavailable;
    private string _statusText = "Library view is idle.";
    private bool _isBusy;
    private bool _isExportBusy;
    private bool _isLoadingMore;
    private bool _isLoadingSelectionDetails;
    private bool _isFilterPanelOpen;
    private string _genreSearchText = string.Empty;
    private EmptyStateKind _emptyStateKind;
    private int _detailsLoadVersion;
    private readonly LibrarySelectionState _selectionState = new(FormatSizeInMegabytes);
    private LibraryStatisticsModel _libraryStatistics = new();
    private readonly Action? _navigateToImport;

    public LibraryBrowserViewModel(
        ILibraryCatalogService? libraryCatalogService = null,
        IPathSelectionService? pathSelectionService = null,
        string? libraryRoot = null,
        ICoverImageLoader? coverImageLoader = null,
        bool hasConfiguredConverter = false,
        BookSortModel? initialSortKey = null,
        bool initialSortDescending = false,
        Action? navigateToImport = null)
    {
        _libraryCatalogService = libraryCatalogService ?? new NullLibraryCatalogService();
        _pathSelectionService = pathSelectionService ?? new NullPathSelectionService();
        _coverPresentationService = new LibraryCoverPresentationService(libraryRoot, coverImageLoader);
        _hasConfiguredConverter = hasConfiguredConverter;
        _navigateToImport = navigateToImport;
        _browseState = new LibraryBrowseQueryState(
            SortKeyOption.For(initialSortKey ?? BookSortModel.Title),
            initialSortDescending);
        _refreshController = new LibraryBrowseRefreshController(_libraryCatalogService, _browseState);
        _selectionWorkflowController = new LibrarySelectionWorkflowController(_libraryCatalogService, _pathSelectionService);
        RefreshCommand = new AsyncCommand(RefreshAsync, () => !IsBusy, HandleCommandErrorAsync);
        LoadDetailsCommand = new AsyncCommand(LoadSelectedBookDetailsAsync, () => HasSelectedBook && !IsLoadingSelectionDetails, HandleCommandErrorAsync);
        ExportSelectedBookCommand = new AsyncCommand(ExportSelectedBookAsync, () => !IsBusy && HasSelectedBook, HandleCommandErrorAsync);
        ExportSelectedBookAsEpubCommand = new AsyncCommand(ExportSelectedBookAsEpubAsync, () => CanExportSelectedBookAsEpub, HandleCommandErrorAsync);
        MoveSelectedBookToTrashCommand = new AsyncCommand(MoveSelectedBookToTrashAsync, () => !IsBusy && HasSelectedBook, HandleCommandErrorAsync);
        SelectBookCommand = new AsyncCommand<BookListItemModel?>(ToggleSelectedBookAsync, book => book is not null);
        CloseSelectionCommand = new AsyncCommand(CloseSelectionAsync, () => HasSelectedBook, HandleCommandErrorAsync);
        ToggleSortDirectionCommand = new AsyncCommand(ToggleSortDirectionAsync, () => true, HandleCommandErrorAsync);
        ClearAllFiltersCommand = new AsyncCommand(ClearAllFiltersAsync, () => true, HandleCommandErrorAsync);
        GoToImportCommand = new AsyncCommand(GoToImportAsync, () => _navigateToImport is not null, HandleCommandErrorAsync);
        LanguageFacets.CollectionChanged += (_, _) =>
        {
            RaisePropertyChanged(nameof(FilteredGenreFacets));
            RaisePropertyChanged(nameof(ActiveFilterCount));
            RaisePropertyChanged(nameof(HasActiveFilters));
            RaisePropertyChanged(nameof(FilterButtonLabel));
        };
        GenreFacets.CollectionChanged += (_, _) =>
        {
            RaisePropertyChanged(nameof(FilteredGenreFacets));
            RaisePropertyChanged(nameof(ActiveFilterCount));
            RaisePropertyChanged(nameof(HasActiveFilters));
            RaisePropertyChanged(nameof(FilterButtonLabel));
        };
    }

    public ObservableCollection<BookListItemModel> Books { get; } = [];
    public ObservableCollection<FilterFacetItem> LanguageFacets { get; } = [];
    public ObservableCollection<FilterFacetItem> GenreFacets { get; } = [];
    public IReadOnlyList<SortKeyOption> AvailableSortKeys { get; } = SortKeyOption.All;
    public AsyncCommand RefreshCommand { get; }
    public AsyncCommand LoadDetailsCommand { get; }
    public AsyncCommand ExportSelectedBookCommand { get; }
    public AsyncCommand ExportSelectedBookAsEpubCommand { get; }
    public AsyncCommand MoveSelectedBookToTrashCommand { get; }
    public AsyncCommand<BookListItemModel?> SelectBookCommand { get; }
    public AsyncCommand CloseSelectionCommand { get; }
    public AsyncCommand ToggleSortDirectionCommand { get; }
    public AsyncCommand ClearAllFiltersCommand { get; }
    public AsyncCommand GoToImportCommand { get; }

    public string SearchText
    {
        get => _browseState.SearchText;
        set
        {
            if (_browseState.SearchText == value)
            {
                return;
            }
            _browseState.SearchText = value;
            RaisePropertyChanged();
            RaisePropertyChanged(nameof(HasActiveFilters));
            ScheduleRefresh();
        }
    }

    public bool IsFilterPanelOpen
    {
        get => _isFilterPanelOpen;
        set => SetProperty(ref _isFilterPanelOpen, value);
    }

    public string GenreSearchText
    {
        get => _genreSearchText;
        set
        {
            if (_genreSearchText == value)
            {
                return;
            }

            _genreSearchText = value;
            RaisePropertyChanged();
            RaisePropertyChanged(nameof(FilteredGenreFacets));
        }
    }

    public IReadOnlyList<FilterFacetItem> FilteredGenreFacets
    {
        get
        {
            if (string.IsNullOrWhiteSpace(_genreSearchText))
            {
                return GenreFacets;
            }

            return GenreFacets
                .Where(f => f.Value.Contains(_genreSearchText, StringComparison.OrdinalIgnoreCase))
                .ToArray();
        }
    }

    public int ActiveFilterCount => _browseState.SelectedLanguages.Count + _browseState.SelectedGenres.Count;

    public bool HasActiveFilters => _browseState.HasActiveFilters;

    public string FilterButtonLabel
    {
        get
        {
            var count = ActiveFilterCount;
            return count == 0 ? "Filters" : $"Filters · {count}";
        }
    }

    public string ActiveFilterCountText
    {
        get
        {
            var count = ActiveFilterCount;
            return count == 0 ? "No filters active" : $"{count} filter{(count == 1 ? "" : "s")} active";
        }
    }

    public SortKeyOption? SelectedSortKey
    {
        get => _browseState.SelectedSortKey;
        set
        {
            if (value is null)
            {
                RaisePropertyChanged(nameof(SelectedSortKey));
                return;
            }

            if (_browseState.SelectedSortKey == value)
            {
                return;
            }

            _browseState.SelectedSortKey = value;
            RaisePropertyChanged(nameof(SelectedSortKey));
            ScheduleRefresh();
        }
    }

    public bool SortDescending
    {
        get => _browseState.SortDescending;
        private set
        {
            if (_browseState.SortDescending == value)
            {
                return;
            }
            _browseState.SortDescending = value;
            RaisePropertyChanged();
            ScheduleRefresh();
        }
    }

    public int PageSize
    {
        get => _browseState.PageSize;
        set
        {
            if (_browseState.PageSize == value)
            {
                return;
            }
            _browseState.PageSize = value;
            RaisePropertyChanged();
        }
    }

    public bool HasMoreResults
    {
        get => _browseState.HasMoreResults;
    }

    public BookListItemModel? SelectedBook
    {
        get => _selectionState.SelectedBook;
        private set
        {
            if (ReferenceEquals(_selectionState.SelectedBook, value))
            {
                return;
            }

            if (_selectionState.SelectedBook is not null)
            {
                _selectionState.SelectedBook.CardPresentation.IsSelected = false;
                ApplySelectionStyle(_selectionState.SelectedBook, isSelected: false);
            }

            var previousSelection = _selectionState.SelectedBook;
            _selectionState.SetSelection(value);
            if (SetProperty(ref previousSelection, value))
            {
                if (_selectionState.SelectedBook is not null)
                {
                    _selectionState.SelectedBook.CardPresentation.IsSelected = true;
                    ApplySelectionStyle(_selectionState.SelectedBook, isSelected: true);
                }

                SelectedBookDetails = null;
                RaiseSelectionPresentationProperties();
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
        get => _selectionState.SelectedBookDetails;
        private set
        {
            var previousDetails = _selectionState.SelectedBookDetails;
            _selectionState.SetDetails(value);
            if (SetProperty(ref previousDetails, value))
            {
                RaiseSelectionPresentationProperties();
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
    public bool ShowExportAsEpubAction => HasSelectedBook && _selectionState.SelectedBookFormat is BookFormatModel.Fb2 && _hasConfiguredConverter;
    public bool ShowExportAsEpubHint => HasSelectedBook && _selectionState.SelectedBookFormat is BookFormatModel.Fb2 && !_hasConfiguredConverter;
    public string ExportAsEpubHintText => "Configure a converter in Settings to enable EPUB export.";
    public bool CanExportSelectedBookAsEpub => !IsBusy && ShowExportAsEpubAction;
    public bool ShowEmptyState => _emptyStateKind != EmptyStateKind.None;
    public bool ShowGoToImportButton => _emptyStateKind == EmptyStateKind.LibraryEmpty;
    public bool ShowClearFiltersButton => _emptyStateKind == EmptyStateKind.NoResults;
    public bool ShowLoadMoreIndicator => HasBooks && IsLoadingMore;
    public string BookCountText => FormatBookCount(_browseState.TotalBookCount);
    public string LibraryStatisticsText =>
        _isLibraryStatisticsUnavailable
            ? "Library summary unavailable."
            : $"Library: {FormatBookCount(_libraryStatistics.BookCount)}, {FormatSizeInMegabytes(_libraryStatistics.TotalLibrarySizeBytes)}";
    public string EmptyStateTitle => _emptyStateKind == EmptyStateKind.NoResults ? "Nothing found" : "Library is empty";
    public string SelectedBookTitle => _selectionState.SelectedBookTitle;
    public string SelectedBookAuthorText => _selectionState.SelectedBookAuthorText;
    public string SelectedBookAnnotationText => _selectionState.SelectedBookAnnotationText;
    public string SelectedBookPathText => _selectionState.SelectedBookPathText;
    public IImage? SelectedBookCoverImage => _selectionState.SelectedBookCoverImage;
    public IBrush? SelectedBookCoverBackgroundBrush => _selectionState.SelectedBookCoverBackgroundBrush;
    public string SelectedBookCoverPlaceholderText => _selectionState.SelectedBookCoverPlaceholderText;
    public bool HasSelectedBookCover => _selectionState.HasSelectedBookCover;
    public bool ShowSelectedBookCoverPlaceholder => _selectionState.ShowSelectedBookCoverPlaceholder;
    public IReadOnlyList<MetadataPair> SelectedBookMetadataPairs => _selectionState.SelectedBookMetadataPairs;

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

        var selectionResult = _selectionWorkflowController.ToggleSelection(SelectedBook, book);
        SelectedBook = selectionResult.SelectedBook;
        StatusText = selectionResult.StatusText;
        if (selectionResult.ShouldLoadDetails)
        {
            await LoadSelectedBookDetailsAsync();
        }
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

        var selectedBook = SelectedBook;
        var detailsLoadVersion = ++_detailsLoadVersion;
        IsLoadingSelectionDetails = true;
        StatusText = $"Loading details for '{selectedBook.Title}'...";

        try
        {
            using var cancellation = CreateOperationCancellationSource(TimeSpan.FromSeconds(10));
            var detailsResult = await _selectionWorkflowController.LoadDetailsAsync(
                selectedBook,
                Prepare,
                cancellation.Token);

            if (IsLifetimeCancellationRequested(cancellation.Token))
            {
                return;
            }

            if (detailsLoadVersion != _detailsLoadVersion || SelectedBook?.BookId != selectedBook.BookId)
            {
                return;
            }

            SelectedBookDetails = detailsResult.Details;
            StatusText = detailsResult.StatusText;
        }
        catch (OperationCanceledException) when (IsLifetimeCancellationRequested())
        {
        }
        finally
        {
            if (!_isDisposed)
            {
                IsLoadingSelectionDetails = detailsLoadVersion == _detailsLoadVersion
                    ? false
                    : IsLoadingSelectionDetails;
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

        var exportPlan = await _selectionWorkflowController.PrepareExportAsync(
            SelectedBook,
            SelectedBookDetails,
            _selectionState.SelectedBookFormat,
            exportFormat,
            default);
        if (!exportPlan.ShouldExport)
        {
            StatusText = exportPlan.StatusText;
            return;
        }

        IsBusy = true;
        IsExportBusy = true;
        StatusText = exportPlan.StatusText;

        try
        {
            using var cancellation = CreateOperationCancellationSource(exportPlan.OperationTimeout);
            var statusText = await _selectionWorkflowController.ExportAsync(
                SelectedBook,
                exportPlan.DestinationPath!,
                exportFormat,
                exportPlan.TransportTimeout,
                cancellation.Token);

            if (IsLifetimeCancellationRequested(cancellation.Token))
            {
                return;
            }

            StatusText = statusText;
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
            var loadedPageCount = Math.Max(_browseState.LoadedPageCount, 1);
            using var cancellation = CreateOperationCancellationSource(TimeSpan.FromSeconds(10));
            var statusText = await _selectionWorkflowController.MoveSelectedBookToTrashAsync(
                SelectedBook,
                () => RefreshLoadedRangeAsync(loadedPageCount),
                cancellation.Token);

            if (IsLifetimeCancellationRequested(cancellation.Token))
            {
                return;
            }

            StatusText = statusText;
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

    private EmptyStateKind ComputeEmptyStateKind() =>
        HasBooks ? EmptyStateKind.None :
        HasActiveFilters ? EmptyStateKind.NoResults :
        EmptyStateKind.LibraryEmpty;

    private void ApplyEmptyState(EmptyStateKind kind)
    {
        if (_emptyStateKind == kind) return;
        _emptyStateKind = kind;
        RaisePropertyChanged(nameof(ShowEmptyState));
        RaisePropertyChanged(nameof(ShowGoToImportButton));
        RaisePropertyChanged(nameof(ShowClearFiltersButton));
        RaisePropertyChanged(nameof(EmptyStateTitle));
    }

    private void ScheduleRefresh()
    {
        if (_isDisposed)
        {
            return;
        }

        // Pending-state transition rules:
        // - LibraryEmpty + filters now active  → show NoResults immediately (no debounce needed for this visual)
        // - LibraryEmpty / NoResults + filters cleared → hide empty state (books may reappear after refresh)
        // - NoResults + filters still active   → keep NoResults; no flicker on each keystroke
        if (_emptyStateKind == EmptyStateKind.LibraryEmpty)
        {
            ApplyEmptyState(HasActiveFilters ? EmptyStateKind.NoResults : EmptyStateKind.None);
        }
        else if (_emptyStateKind == EmptyStateKind.NoResults && !HasActiveFilters)
        {
            ApplyEmptyState(EmptyStateKind.None);
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
        var previousHasMoreResults = HasMoreResults;

        try
        {
            using var cancellation = CreateOperationCancellationSource(TimeSpan.FromSeconds(10));
            var refreshResult = await _refreshController.RefreshBatchAsync(
                batchNumber,
                TimeSpan.FromSeconds(5),
                cancellation.Token,
                Prepare);

            if (IsLifetimeCancellationRequested(cancellation.Token))
            {
                return;
            }

            await ApplyRefreshResultAsync(refreshResult, previousSelectionId, previousHasMoreResults);
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
        await RefreshRangeAsync(_browseState.BuildRangeItemLimit(loadedPageCount));
    }

    private async Task AppendNextBatchAsync()
    {
        if (_isDisposed)
        {
            return;
        }

        if (_browseState.LoadedPageCount <= 0)
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
            var nextBatchNumber = _browseState.LoadedPageCount + 1;
            var refreshResult = await _refreshController.LoadMoreAsync(
                nextBatchNumber,
                TimeSpan.FromSeconds(5),
                _loadMoreCancellation.Token,
                Prepare);

            if (IsLifetimeCancellationRequested(_loadMoreCancellation.Token))
            {
                return;
            }

            foreach (var item in refreshResult.VisibleItems)
            {
                if (Books.Any(existing => existing.BookId == item.BookId))
                {
                    continue;
                }

                Books.Add(item);
            }

            var previousHasMoreResults = HasMoreResults;
            _browseState.SetLoadedState(nextBatchNumber, refreshResult.TotalCount, Books.Count);
            _isLibraryStatisticsUnavailable = ApplyLibraryStatistics(refreshResult.Statistics);
            SyncHasMoreResults(previousHasMoreResults);
            UpdateAvailableLanguages(refreshResult.AvailableLanguages);
            UpdateAvailableGenres(refreshResult.AvailableGenres);
            RaisePropertyChanged(nameof(HasBooks));
            ApplyEmptyState(ComputeEmptyStateKind());
            RaisePropertyChanged(nameof(BookCountText));
            RaisePropertyChanged(nameof(LibraryStatisticsText));
            StatusText = BuildRefreshStatusText(Books.Count, _browseState.TotalBookCount, _isLibraryStatisticsUnavailable);
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

    private bool ApplyLibraryStatistics(LibraryStatisticsModel? statistics)
    {
        if (statistics is not null)
        {
            _libraryStatistics = statistics;
            return false;
        }

        UiLogging.Warning("ListBooks response did not include library statistics; library summary will remain unavailable.");
        return true;
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
        var previousHasMoreResults = HasMoreResults;

        try
        {
            using var cancellation = CreateOperationCancellationSource(TimeSpan.FromSeconds(10));
            var refreshResult = await _refreshController.RefreshRangeAsync(
                itemLimit,
                TimeSpan.FromSeconds(5),
                cancellation.Token,
                Prepare);

            if (IsLifetimeCancellationRequested(cancellation.Token))
            {
                return;
            }

            await ApplyRefreshResultAsync(refreshResult, previousSelectionId, previousHasMoreResults);
        }
        finally
        {
            if (!_isDisposed)
            {
                IsBusy = false;
            }
        }
    }

    private Task ApplyRefreshResultAsync(
        LibraryBrowseRefreshResult refreshResult,
        long? previousSelectionId,
        bool previousHasMoreResults)
    {
        ApplyVisibleItems(refreshResult.VisibleItems);
        UpdateAvailableLanguages(refreshResult.AvailableLanguages);
        UpdateAvailableGenres(refreshResult.AvailableGenres);

        if (previousSelectionId is not null)
        {
            var preservedDetails = SelectedBookDetails;
            SelectedBook = refreshResult.VisibleItems.FirstOrDefault(item => item.BookId == previousSelectionId);
            if (SelectedBook?.BookId == previousSelectionId && preservedDetails is not null)
            {
                SelectedBookDetails = preservedDetails;
            }
        }
        else
        {
            SelectedBook = null;
        }

        _isLibraryStatisticsUnavailable = ApplyLibraryStatistics(refreshResult.Statistics);
        SyncHasMoreResults(previousHasMoreResults);
        RaisePropertyChanged(nameof(HasBooks));
        ApplyEmptyState(ComputeEmptyStateKind());
        RaisePropertyChanged(nameof(BookCountText));
        RaisePropertyChanged(nameof(LibraryStatisticsText));
        StatusText = BuildRefreshStatusText(Books.Count, _browseState.TotalBookCount, _isLibraryStatisticsUnavailable);
        return Task.CompletedTask;
    }

    private void ApplyVisibleItems(IReadOnlyList<BookListItemModel> visibleItems)
    {
        Books.Clear();
        foreach (var item in visibleItems)
        {
            Books.Add(item);
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

    private Task ToggleSortDirectionAsync()
    {
        SortDescending = !SortDescending;
        return Task.CompletedTask;
    }

    private BookListItemModel Prepare(BookListItemModel item)
    {
        _coverPresentationService.Prepare(item);
        item.CardPresentation.IsSelected = false;
        ApplySelectionStyle(item, isSelected: false);
        return item;
    }

    private BookDetailsModel Prepare(BookDetailsModel item)
    {
        return _coverPresentationService.Prepare(item);
    }

    private void RaiseSelectionPresentationProperties()
    {
        RaisePropertyChanged(nameof(HasSelectedBook));
        RaisePropertyChanged(nameof(ShowDetailsPanel));
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
    }

    private void UpdateAvailableLanguages(IEnumerable<FacetItemModel> availableLanguages)
    {
        var desired = LibraryBrowseQueryState.BuildAvailableValues(availableLanguages.Select(f => f.Value), _browseState.SelectedLanguages);
        SynchronizeFacets(LanguageFacets, availableLanguages, desired, _browseState.SelectedLanguages);
    }

    private void UpdateAvailableGenres(IEnumerable<FacetItemModel> availableGenres)
    {
        var desired = LibraryBrowseQueryState.BuildAvailableValues(availableGenres.Select(f => f.Value), _browseState.SelectedGenres);
        SynchronizeFacets(GenreFacets, availableGenres, desired, _browseState.SelectedGenres);
        RaisePropertyChanged(nameof(FilteredGenreFacets));
    }

    private void SynchronizeFacets(
        ObservableCollection<FilterFacetItem> facets,
        IEnumerable<FacetItemModel> facetModels,
        IReadOnlyList<string> desiredValues,
        IReadOnlyList<string> selectedValues)
    {
        var countLookup = BuildFacetCountLookup(facetModels);
        var selectedLookup = new HashSet<string>(selectedValues, StringComparer.OrdinalIgnoreCase);

        for (var i = facets.Count - 1; i >= 0; i--)
        {
            if (!desiredValues.Contains(facets[i].Value, StringComparer.OrdinalIgnoreCase))
            {
                facets[i].PropertyChanged -= OnFacetSelectionChanged;
                facets.RemoveAt(i);
            }
        }

        for (var di = 0; di < desiredValues.Count; di++)
        {
            var value = desiredValues[di];
            var existingIndex = -1;
            for (var ei = 0; ei < facets.Count; ei++)
            {
                if (string.Equals(facets[ei].Value, value, StringComparison.OrdinalIgnoreCase))
                {
                    existingIndex = ei;
                    break;
                }
            }

            var count = countLookup.TryGetValue(value, out var c) ? c : 0u;

            if (existingIndex < 0)
            {
                var item = new FilterFacetItem(value, count)
                {
                    IsSelected = selectedLookup.Contains(value)
                };
                item.PropertyChanged += OnFacetSelectionChanged;
                facets.Insert(di, item);
            }
            else
            {
                if (existingIndex != di)
                {
                    facets.Move(existingIndex, di);
                }

                if (facets[di].BookCount != count)
                {
                    facets[di].BookCount = count;
                }
            }

            if (facets[di].IsSelected != selectedLookup.Contains(facets[di].Value))
            {
                facets[di].IsSelected = selectedLookup.Contains(facets[di].Value);
            }
        }
    }

    private static Dictionary<string, uint> BuildFacetCountLookup(IEnumerable<FacetItemModel> facetModels)
    {
        var countLookup = new Dictionary<string, uint>(StringComparer.OrdinalIgnoreCase);

        foreach (var facet in facetModels)
        {
            if (string.IsNullOrWhiteSpace(facet.Value))
            {
                continue;
            }

            if (countLookup.TryGetValue(facet.Value, out var existingCount))
            {
                countLookup[facet.Value] = checked(existingCount + facet.Count);
                continue;
            }

            countLookup.Add(facet.Value, facet.Count);
        }

        return countLookup;
    }

    private void OnFacetSelectionChanged(object? sender, System.ComponentModel.PropertyChangedEventArgs e)
    {
        if (e.PropertyName != nameof(FilterFacetItem.IsSelected))
        {
            return;
        }

        _browseState.SelectedLanguages = LanguageFacets
            .Where(f => f.IsSelected)
            .Select(f => f.Value)
            .ToArray();
        _browseState.SelectedGenres = GenreFacets
            .Where(f => f.IsSelected)
            .Select(f => f.Value)
            .ToArray();

        RaisePropertyChanged(nameof(ActiveFilterCount));
        RaisePropertyChanged(nameof(HasActiveFilters));
        RaisePropertyChanged(nameof(FilterButtonLabel));
        RaisePropertyChanged(nameof(ActiveFilterCountText));
        RaisePropertyChanged(nameof(BookCountText));
        ScheduleRefresh();
    }

    private Task ClearAllFiltersAsync()
    {
        foreach (var facet in LanguageFacets)
        {
            facet.IsSelected = false;
        }

        foreach (var facet in GenreFacets)
        {
            facet.IsSelected = false;
        }

        _browseState.SelectedLanguages = [];
        _browseState.SelectedGenres = [];
        GenreSearchText = string.Empty;

        RaisePropertyChanged(nameof(ActiveFilterCount));
        RaisePropertyChanged(nameof(HasActiveFilters));
        RaisePropertyChanged(nameof(FilterButtonLabel));
        RaisePropertyChanged(nameof(ActiveFilterCountText));
        RaisePropertyChanged(nameof(BookCountText));
        ScheduleRefresh();
        return Task.CompletedTask;
    }

    private Task GoToImportAsync()
    {
        _navigateToImport?.Invoke();
        return Task.CompletedTask;
    }

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

    private void SyncHasMoreResults(bool previousValue)
    {
        if (previousValue == _browseState.HasMoreResults)
        {
            return;
        }

        RaisePropertyChanged(nameof(HasMoreResults));
    }

    private static void ApplySelectionStyle(BookListItemModel item, bool isSelected)
    {
        item.CardPresentation.CardBackgroundBrush = isSelected ? SelectedCardBackground : DefaultCardBackground;
        item.CardPresentation.CardBorderBrush = isSelected ? SelectedCardBorder : DefaultCardBorder;
        item.CardPresentation.CardBorderThickness = isSelected ? new Thickness(2) : new Thickness(0);
    }
}
