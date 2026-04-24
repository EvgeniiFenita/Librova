using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Markup.Xaml;
using Avalonia.Media;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Librova.UI.LibraryCatalog;
using Librova.UI.ViewModels;
using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.ComponentModel;
using System.Linq;

namespace Librova.UI.Views;

internal sealed partial class LibraryView : UserControl
{
    private const int MaxCollectionContextMenuNameLength = 36;
    private const string CollectionContextMenuEllipsis = "...";

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

    private void OnBookCardContextRequested(object? sender, ContextRequestedEventArgs eventArgs)
    {
        if (sender is not Button { DataContext: BookListItemModel requestedBook } button || _libraryBrowser is null)
        {
            return;
        }

        if (_libraryBrowser.SelectedBook?.BookId != requestedBook.BookId
            && _libraryBrowser.SelectBookCommand.CanExecute(requestedBook))
        {
            _libraryBrowser.SelectBookCommand.Execute(requestedBook);
        }

        var menu = BuildBookCardContextMenu(_libraryBrowser, requestedBook);
        button.ContextMenu = menu;
        menu.Open(button);
        eventArgs.Handled = true;
    }

    internal static ContextMenu BuildBookCardContextMenu(LibraryBrowserViewModel libraryBrowser, BookListItemModel requestedBook)
    {
        var menuItems = new List<object>
        {
            CreateCommandMenuItem("Export", libraryBrowser.ExportBookFromCardCommand, requestedBook, "IconExport")
        };

        if (libraryBrowser.ExportBookAsEpubFromCardCommand.CanExecute(requestedBook))
        {
            menuItems.Add(CreateCommandMenuItem("Export as EPUB", libraryBrowser.ExportBookAsEpubFromCardCommand, requestedBook, "IconExport"));
        }

        var membershipItems = new List<object>();
        var membershipMenu = new MenuItem
        {
            Header = "Add to",
            Icon = CreateMenuIcon("IconAddFolder"),
            Classes = { "BookCardContextItem" }
        };

        foreach (var collection in libraryBrowser.Collections)
        {
            var isMember = requestedBook.Collections.Any(item => item.CollectionId == collection.CollectionId);
            var membershipItem = new MenuItem
            {
                Header = FormatCollectionContextMenuHeader(collection.Name, isMember),
                Command = libraryBrowser.ToggleBookCollectionMembershipCommand,
                CommandParameter = new BookCollectionMembershipRequest(requestedBook, collection.Collection, isMember),
                IsEnabled = !isMember,
                Icon = CreateCollectionGlyphIcon(collection.IconGlyph),
                Classes = { "BookCardContextItem" }
            };
            ToolTip.SetTip(membershipItem, collection.Name);
            membershipItems.Add(membershipItem);
        }

        membershipItems.Add(new Separator
        {
            Classes = { "BookCardContextSeparator" }
        });
        var createNewItem = new MenuItem
        {
            Header = "Create new...",
            Classes = { "BookCardContextItem" }
        };
        createNewItem.Click += (_, _) => libraryBrowser.StartCreateCollectionForBook(requestedBook);
        membershipItems.Add(createNewItem);
        foreach (var item in membershipItems)
        {
            membershipMenu.Items.Add(item);
        }

        menuItems.Add(membershipMenu);

        if (libraryBrowser.IsCollectionViewActive)
        {
            menuItems.Add(CreateCommandMenuItem("Remove from collection", libraryBrowser.RemoveBookFromSelectedCollectionCommand, requestedBook, "IconClose"));
        }

        menuItems.Add(CreateCommandMenuItem("Copy Title", libraryBrowser.CopyBookTitleCommand, requestedBook, "IconCopy"));
        menuItems.Add(new Separator
        {
            Classes = { "BookCardContextSeparator" }
        });
        menuItems.Add(CreateCommandMenuItem("Move to Trash", libraryBrowser.MoveBookToTrashFromCardCommand, requestedBook, "IconTrash", isDestructive: true));

        return new ContextMenu
        {
            ItemsSource = menuItems,
            Classes = { "BookCardContextMenu" }
        };
    }

    private static MenuItem CreateCommandMenuItem(
        string header,
        System.Windows.Input.ICommand command,
        object parameter,
        string iconResourceKey,
        bool isDestructive = false)
    {
        var menuItem = new MenuItem
        {
            Header = header,
            Command = command,
            CommandParameter = parameter,
            Icon = CreateMenuIcon(iconResourceKey)
        };
        menuItem.Classes.Add("BookCardContextItem");
        if (isDestructive)
        {
            menuItem.Classes.Add("Destructive");
        }

        return menuItem;
    }

    private static string FormatCollectionContextMenuHeader(string name, bool isMember)
    {
        var displayName = TruncateCollectionContextMenuName(name);
        return isMember ? $"✓ {displayName}" : displayName;
    }

    private static string TruncateCollectionContextMenuName(string name)
    {
        var normalizedName = string.IsNullOrWhiteSpace(name) ? "Untitled collection" : name.Trim();
        if (normalizedName.Length <= MaxCollectionContextMenuNameLength)
        {
            return normalizedName;
        }

        return normalizedName[..(MaxCollectionContextMenuNameLength - CollectionContextMenuEllipsis.Length)]
            + CollectionContextMenuEllipsis;
    }

    private static PathIcon CreateMenuIcon(string iconResourceKey)
    {
        var icon = new PathIcon
        {
            Width = 16,
            Height = 16
        };
        if (Application.Current?.FindResource(iconResourceKey) is Geometry geometry)
        {
            icon.Data = geometry;
        }

        return icon;
    }

    private static TextBlock CreateCollectionGlyphIcon(string glyph) =>
        new()
        {
            Text = glyph,
            FontSize = 15,
            Width = 18,
            Height = 18,
            TextAlignment = TextAlignment.Center,
            VerticalAlignment = Avalonia.Layout.VerticalAlignment.Center
        };

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
