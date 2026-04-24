using Librova.UI.LibraryCatalog;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Librova.UI.ViewModels;

internal sealed class LibraryBrowseQueryState
{
    public LibraryBrowseQueryState(SortKeyOption initialSortKey, bool initialSortDescending)
    {
        SelectedSortKey = initialSortKey;
        SortDescending = initialSortDescending;
    }

    public string SearchText { get; set; } = string.Empty;
    public IReadOnlyList<string> SelectedLanguages { get; set; } = [];
    public IReadOnlyList<string> SelectedGenres { get; set; } = [];
    public long? SelectedCollectionId { get; set; }
    public SortKeyOption SelectedSortKey { get; set; }
    public bool SortDescending { get; set; }

    private int _pageSize = 120;
    public int PageSize
    {
        get => _pageSize;
        set => _pageSize = value < 1 ? 1 : value;
    }

    public int LoadedPageCount { get; private set; }
    public bool HasMoreResults { get; set; }
    public ulong TotalBookCount { get; private set; }

    public bool HasActiveFilters =>
        !string.IsNullOrWhiteSpace(SearchText)
        || SelectedLanguages.Count > 0
        || SelectedGenres.Count > 0;

    public bool HasResultRestrictions =>
        HasActiveFilters
        || SelectedCollectionId is not null;

    public BookListRequestModel BuildBatchRequest(int batchNumber) =>
        new()
        {
            Text = SearchText,
            Languages = SelectedLanguages,
            Genres = SelectedGenres,
            CollectionId = SelectedCollectionId,
            SortBy = SelectedSortKey.Key,
            SortDirection = SortDescending ? BookSortDirectionModel.Descending : BookSortDirectionModel.Ascending,
            Offset = checked((ulong)Math.Max(0, batchNumber - 1) * (ulong)PageSize),
            Limit = (ulong)PageSize
        };

    public BookListRequestModel BuildInitialRangeRequest(int itemLimit) =>
        new()
        {
            Text = SearchText,
            Languages = SelectedLanguages,
            Genres = SelectedGenres,
            CollectionId = SelectedCollectionId,
            SortBy = SelectedSortKey.Key,
            SortDirection = SortDescending ? BookSortDirectionModel.Descending : BookSortDirectionModel.Ascending,
            Offset = 0,
            Limit = (ulong)Math.Max(1, itemLimit)
        };

    public int BuildRangeItemLimit(int loadedPageCount) => Math.Max(1, loadedPageCount) * PageSize;

    public void SetLoadedState(int loadedPageCount, ulong totalBookCount, int visibleBookCount)
    {
        LoadedPageCount = loadedPageCount;
        TotalBookCount = totalBookCount;
        HasMoreResults = (ulong)visibleBookCount < totalBookCount;
    }

    public void SetLoadedRangeState(int visibleBookCount, ulong totalBookCount)
    {
        LoadedPageCount = Math.Max(1, (visibleBookCount + PageSize - 1) / PageSize);
        TotalBookCount = totalBookCount;
        HasMoreResults = (ulong)visibleBookCount < totalBookCount;
    }

    public static IReadOnlyList<string> BuildAvailableValues(
        IEnumerable<string> incomingValues,
        IEnumerable<string>? selectedValues = null) =>
        incomingValues
            .Concat(selectedValues ?? [])
            .Where(value => !string.IsNullOrWhiteSpace(value))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(value => value, StringComparer.OrdinalIgnoreCase)
            .ToArray();
}
