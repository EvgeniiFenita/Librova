using Avalonia;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Librova.UI.LibraryCatalog;
using Librova.UI.ViewModels;
using System;
using System.Collections.Specialized;
using System.ComponentModel;
using System.Linq;

namespace Librova.UI.Views;

internal sealed partial class LibraryView : UserControl
{
    private ScrollViewer? _booksViewport;
    private LibraryBrowserViewModel? _libraryBrowser;
    private long? _selectionAnchorBookId;
    private int _pendingCenterRequestId;
    private bool _wasDetailsPanelVisible;
    private bool _isOpenPanelSnapshotInvalidated;
    private bool _suppressViewportOffsetTracking;
    private SelectionViewportSnapshot? _openPanelSnapshot;

    public LibraryView()
    {
        AvaloniaXamlLoader.Load(this);
        _booksViewport = this.FindControl<ScrollViewer>("BooksViewport");
        if (_booksViewport is not null)
        {
            _booksViewport.PropertyChanged += OnBooksViewportPropertyChanged;
        }

        var filterPopup = this.FindControl<Avalonia.Controls.Primitives.Popup>("FilterPopup");
        var filterBtn = this.FindControl<Avalonia.Controls.Primitives.ToggleButton>("FilterBtn");
        if (filterPopup is not null && filterBtn is not null)
        {
            filterPopup.PlacementTarget = filterBtn;
        }

        DataContextChanged += OnDataContextChanged;
        AttachToViewModel(DataContext as ShellViewModel);
    }

    private void OnDataContextChanged(object? sender, System.EventArgs eventArgs)
    {
        AttachToViewModel(DataContext as ShellViewModel);
    }

    private void AttachToViewModel(ShellViewModel? shell)
    {
        if (_libraryBrowser is not null)
        {
            _libraryBrowser.PropertyChanged -= OnLibraryBrowserPropertyChanged;
            _libraryBrowser.Books.CollectionChanged -= OnBooksCollectionChanged;
        }

        _libraryBrowser = shell?.LibraryBrowser;
        _selectionAnchorBookId = _libraryBrowser?.SelectedBook?.BookId;
        _wasDetailsPanelVisible = _libraryBrowser?.ShowDetailsPanel ?? false;
        _openPanelSnapshot = null;
        _isOpenPanelSnapshotInvalidated = false;

        if (_libraryBrowser is not null)
        {
            _libraryBrowser.PropertyChanged += OnLibraryBrowserPropertyChanged;
            _libraryBrowser.Books.CollectionChanged += OnBooksCollectionChanged;
        }
    }

    private void OnLibraryBrowserPropertyChanged(object? sender, PropertyChangedEventArgs eventArgs)
    {
        if (sender is not LibraryBrowserViewModel browser)
        {
            return;
        }

        if (eventArgs.PropertyName is nameof(LibraryBrowserViewModel.SearchText)
            or nameof(LibraryBrowserViewModel.ActiveFilterCount)
            or nameof(LibraryBrowserViewModel.SelectedSortKey)
            or nameof(LibraryBrowserViewModel.SortDescending))
        {
            InvalidateOpenPanelSnapshot();
            if (eventArgs.PropertyName is nameof(LibraryBrowserViewModel.SelectedSortKey)
                or nameof(LibraryBrowserViewModel.SortDescending))
            {
                ResetScrollToTop();
            }
            return;
        }

        if (eventArgs.PropertyName is not nameof(LibraryBrowserViewModel.SelectedBook)
            and not nameof(LibraryBrowserViewModel.ShowDetailsPanel))
        {
            return;
        }

        var detailsVisible = browser.ShowDetailsPanel;
        if (browser.SelectedBook is not null)
        {
            if (_wasDetailsPanelVisible
                && _openPanelSnapshot is not null
                && _openPanelSnapshot.BookId != browser.SelectedBook.BookId)
            {
                InvalidateOpenPanelSnapshot();
            }

            _selectionAnchorBookId = browser.SelectedBook.BookId;
        }

        if (!_wasDetailsPanelVisible && detailsVisible)
        {
            CaptureOpenPanelSnapshot(browser);
            ScheduleSelectionPositioning(SelectionViewportAction.Center);
        }
        else if (_wasDetailsPanelVisible && detailsVisible && _isOpenPanelSnapshotInvalidated && browser.SelectedBook is not null)
        {
            // Panel stays open (same or new book after sort/filter reset) but scroll was reset;
            // ensure the selected book is visible again.
            ScheduleSelectionPositioning(SelectionViewportAction.EnsureVisible);
        }
        else if (_wasDetailsPanelVisible && !detailsVisible)
        {
            if (CanRestoreOpenPanelSnapshot())
            {
                ScheduleViewportRestore(_openPanelSnapshot!);
            }
            else
            {
                ScheduleSelectionPositioning(SelectionViewportAction.EnsureVisible);
            }

            _openPanelSnapshot = null;
            _isOpenPanelSnapshotInvalidated = false;
        }

        _wasDetailsPanelVisible = detailsVisible;
    }

    private void OnBooksCollectionChanged(object? sender, NotifyCollectionChangedEventArgs eventArgs)
    {
        if (_wasDetailsPanelVisible)
        {
            InvalidateOpenPanelSnapshot();
        }
    }

    private void OnViewportOffsetChanged(Vector offset)
    {
        TryLoadMoreBooks();

        if (_suppressViewportOffsetTracking || !_wasDetailsPanelVisible || _openPanelSnapshot is null)
        {
            return;
        }

        if (Math.Abs(offset.Y - _openPanelSnapshot.Offset.Y) < 0.5d)
        {
            return;
        }

        InvalidateOpenPanelSnapshot();
    }

    private void OnBooksViewportPropertyChanged(object? sender, AvaloniaPropertyChangedEventArgs eventArgs)
    {
        if (eventArgs.Property != ScrollViewer.OffsetProperty)
        {
            return;
        }

        if (eventArgs.NewValue is Vector offset)
        {
            OnViewportOffsetChanged(offset);
        }
    }

    private void CaptureOpenPanelSnapshot(LibraryBrowserViewModel browser)
    {
        _booksViewport ??= this.FindControl<ScrollViewer>("BooksViewport");
        if (_booksViewport is null || browser.SelectedBook is null)
        {
            _openPanelSnapshot = null;
            _isOpenPanelSnapshotInvalidated = true;
            return;
        }

        _openPanelSnapshot = new SelectionViewportSnapshot(browser.SelectedBook.BookId, _booksViewport.Offset);
        _isOpenPanelSnapshotInvalidated = false;
    }

    private void InvalidateOpenPanelSnapshot()
    {
        if (_openPanelSnapshot is not null)
        {
            _isOpenPanelSnapshotInvalidated = true;
        }
    }

    private bool CanRestoreOpenPanelSnapshot() =>
        _openPanelSnapshot is not null
        && !_isOpenPanelSnapshotInvalidated
        && _selectionAnchorBookId is not null
        && _openPanelSnapshot.BookId == _selectionAnchorBookId.Value;

    private void ScheduleSelectionPositioning(SelectionViewportAction action)
    {
        if (_selectionAnchorBookId is null)
        {
            return;
        }

        var requestId = ++_pendingCenterRequestId;
        Dispatcher.UIThread.Post(
            () => TryPositionSelectionInViewport(requestId, _selectionAnchorBookId.Value, action, attempt: 0),
            DispatcherPriority.Loaded);
    }

    private void ScheduleViewportRestore(SelectionViewportSnapshot snapshot)
    {
        var requestId = ++_pendingCenterRequestId;
        Dispatcher.UIThread.Post(
            () => TryRestoreViewportSnapshot(requestId, snapshot, attempt: 0),
            DispatcherPriority.Loaded);
    }

    private void TryPositionSelectionInViewport(int requestId, long bookId, SelectionViewportAction action, int attempt)
    {
        if (requestId != _pendingCenterRequestId)
        {
            return;
        }

        _booksViewport ??= this.FindControl<ScrollViewer>("BooksViewport");
        if (_booksViewport is null || !_booksViewport.IsVisible || _booksViewport.Bounds.Height <= 0)
        {
            RetrySelectionPositioning(requestId, bookId, action, attempt);
            return;
        }

        var selectedCard = this.GetVisualDescendants()
            .OfType<Button>()
            .FirstOrDefault(button =>
                button.Classes.Contains("BookCardButton")
                && button.DataContext is BookListItemModel item
                && item.BookId == bookId);
        if (selectedCard is null || selectedCard.Bounds.Height <= 0)
        {
            RetrySelectionPositioning(requestId, bookId, action, attempt);
            return;
        }

        var topLeft = selectedCard.TranslatePoint(new Point(0, 0), _booksViewport);
        if (topLeft is null)
        {
            RetrySelectionPositioning(requestId, bookId, action, attempt);
            return;
        }

        var targetOffsetY = action is SelectionViewportAction.Center
            ? LibrarySelectionViewportCentering.CalculateCenteredVerticalOffset(
                _booksViewport.Offset.Y,
                _booksViewport.Bounds.Height,
                _booksViewport.Extent.Height,
                topLeft.Value.Y,
                selectedCard.Bounds.Height)
            : LibrarySelectionViewportCentering.CalculateVisibleVerticalOffset(
                _booksViewport.Offset.Y,
                _booksViewport.Bounds.Height,
                _booksViewport.Extent.Height,
                topLeft.Value.Y,
                selectedCard.Bounds.Height);

        if (Math.Abs(targetOffsetY - _booksViewport.Offset.Y) < 0.5d)
        {
            return;
        }

        SetViewportOffset(new Vector(_booksViewport.Offset.X, targetOffsetY));
    }

    private void TryRestoreViewportSnapshot(int requestId, SelectionViewportSnapshot snapshot, int attempt)
    {
        if (requestId != _pendingCenterRequestId)
        {
            return;
        }

        _booksViewport ??= this.FindControl<ScrollViewer>("BooksViewport");
        if (_booksViewport is null || !_booksViewport.IsVisible || _booksViewport.Bounds.Height <= 0)
        {
            RetryViewportRestore(requestId, snapshot, attempt);
            return;
        }

        var targetOffsetY = LibrarySelectionViewportCentering.ClampVerticalOffset(
            snapshot.Offset.Y,
            _booksViewport.Bounds.Height,
            _booksViewport.Extent.Height);
        if (Math.Abs(targetOffsetY - _booksViewport.Offset.Y) < 0.5d)
        {
            return;
        }

        SetViewportOffset(new Vector(_booksViewport.Offset.X, targetOffsetY));
    }

    private void RetrySelectionPositioning(int requestId, long bookId, SelectionViewportAction action, int attempt)
    {
        if (attempt >= 2)
        {
            return;
        }

        Dispatcher.UIThread.Post(
            () => TryPositionSelectionInViewport(requestId, bookId, action, attempt + 1),
            DispatcherPriority.Background);
    }

    private void RetryViewportRestore(int requestId, SelectionViewportSnapshot snapshot, int attempt)
    {
        if (attempt >= 2)
        {
            return;
        }

        Dispatcher.UIThread.Post(
            () => TryRestoreViewportSnapshot(requestId, snapshot, attempt + 1),
            DispatcherPriority.Background);
    }

    private void SetViewportOffset(Vector offset)
    {
        if (_booksViewport is null)
        {
            return;
        }

        _suppressViewportOffsetTracking = true;
        try
        {
            _booksViewport.Offset = offset;
        }
        finally
        {
            _suppressViewportOffsetTracking = false;
        }
    }

    private void TryLoadMoreBooks()
    {
        if (_booksViewport is null || _libraryBrowser is null)
        {
            return;
        }

        if (!_libraryBrowser.HasMoreResults || _libraryBrowser.IsBusy || _libraryBrowser.IsLoadingMore)
        {
            return;
        }

        const double preloadThreshold = 320d;
        var remainingHeight = _booksViewport.Extent.Height - (_booksViewport.Offset.Y + _booksViewport.Viewport.Height);
        if (remainingHeight > preloadThreshold)
        {
            return;
        }

        _ = _libraryBrowser.LoadMoreAsync();
    }

    private void ResetScrollToTop()
    {
        _booksViewport ??= this.FindControl<ScrollViewer>("BooksViewport");
        if (_booksViewport is not null)
        {
            SetViewportOffset(new Vector(0, 0));
        }
    }

    private enum SelectionViewportAction
    {
        Center,
        EnsureVisible
    }

    private sealed record SelectionViewportSnapshot(long BookId, Vector Offset);
}
