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
        AvailableLanguageFilters = ["All languages"];
    }

    public string SearchText { get; set; } = string.Empty;
    public string LanguageFilter { get; set; } = string.Empty;
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
    public IReadOnlyList<string> AvailableLanguageFilters { get; private set; }

    public string SelectedLanguageFilter => string.IsNullOrWhiteSpace(LanguageFilter) ? "All languages" : LanguageFilter;

    public bool HasActiveFilters =>
        !string.IsNullOrWhiteSpace(SearchText)
        || !string.IsNullOrWhiteSpace(LanguageFilter);

    public string NormalizeSelectedLanguageFilter(string value) =>
        string.Equals(value, "All languages", StringComparison.OrdinalIgnoreCase)
            ? string.Empty
            : value;

    public BookListRequestModel BuildBatchRequest(int batchNumber) =>
        new()
        {
            Text = SearchText,
            Language = string.IsNullOrWhiteSpace(LanguageFilter) ? null : LanguageFilter,
            SortBy = SelectedSortKey.Key,
            SortDirection = SortDescending ? BookSortDirectionModel.Descending : BookSortDirectionModel.Ascending,
            Offset = checked((ulong)Math.Max(0, batchNumber - 1) * (ulong)PageSize),
            Limit = (ulong)PageSize
        };

    public BookListRequestModel BuildInitialRangeRequest(int itemLimit) =>
        new()
        {
            Text = SearchText,
            Language = string.IsNullOrWhiteSpace(LanguageFilter) ? null : LanguageFilter,
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

    public void UpdateAvailableLanguages(IEnumerable<string> availableLanguages)
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

        AvailableLanguageFilters = items.Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
    }
}
