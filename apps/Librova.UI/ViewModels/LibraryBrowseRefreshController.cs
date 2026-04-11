using Librova.UI.LibraryCatalog;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class LibraryBrowseRefreshController
{
    private readonly ILibraryCatalogService _libraryCatalogService;
    private readonly LibraryBrowseQueryState _browseState;

    public LibraryBrowseRefreshController(
        ILibraryCatalogService libraryCatalogService,
        LibraryBrowseQueryState browseState)
    {
        _libraryCatalogService = libraryCatalogService;
        _browseState = browseState;
    }

    public async Task<LibraryBrowseRefreshResult> RefreshBatchAsync(
        int batchNumber,
        TimeSpan timeout,
        CancellationToken cancellationToken,
        Func<BookListItemModel, BookListItemModel> prepare)
    {
        var page = await _libraryCatalogService.ListBooksAsync(
            _browseState.BuildBatchRequest(batchNumber),
            timeout,
            cancellationToken);

        var visibleItems = page.Items.Select(prepare).ToArray();
        _browseState.SetLoadedState(batchNumber, page.TotalCount, visibleItems.Length);
        return new LibraryBrowseRefreshResult(
            visibleItems,
            page.TotalCount,
            page.AvailableLanguages,
            page.AvailableGenres,
            page.Statistics);
    }

    public async Task<LibraryBrowseRefreshResult> LoadMoreAsync(
        int batchNumber,
        TimeSpan timeout,
        CancellationToken cancellationToken,
        Func<BookListItemModel, BookListItemModel> prepare)
    {
        var page = await _libraryCatalogService.ListBooksAsync(
            _browseState.BuildBatchRequest(batchNumber),
            timeout,
            cancellationToken);

        var visibleItems = page.Items.Select(prepare).ToArray();
        return new LibraryBrowseRefreshResult(
            visibleItems,
            page.TotalCount,
            page.AvailableLanguages,
            page.AvailableGenres,
            page.Statistics);
    }

    public async Task<LibraryBrowseRefreshResult> RefreshRangeAsync(
        int itemLimit,
        TimeSpan timeout,
        CancellationToken cancellationToken,
        Func<BookListItemModel, BookListItemModel> prepare)
    {
        var page = await _libraryCatalogService.ListBooksAsync(
            _browseState.BuildInitialRangeRequest(itemLimit),
            timeout,
            cancellationToken);

        var visibleItems = page.Items.Select(prepare).ToArray();
        _browseState.SetLoadedRangeState(visibleItems.Length, page.TotalCount);
        return new LibraryBrowseRefreshResult(
            visibleItems,
            page.TotalCount,
            page.AvailableLanguages,
            page.AvailableGenres,
            page.Statistics);
    }
}

internal sealed record LibraryBrowseRefreshResult(
    IReadOnlyList<BookListItemModel> VisibleItems,
    ulong TotalCount,
    IReadOnlyList<string> AvailableLanguages,
    IReadOnlyList<string> AvailableGenres,
    LibraryStatisticsModel? Statistics);
