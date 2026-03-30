using System;
using System.Collections.Generic;

namespace Librova.UI.LibraryCatalog;

internal enum BookFormatModel
{
    Epub,
    Fb2
}

internal enum BookSortModel
{
    Title,
    Author,
    DateAdded,
    Series,
    Year,
    FileSize
}

internal sealed class BookListRequestModel
{
    public string Text { get; init; } = string.Empty;
    public string? Author { get; init; }
    public string? Language { get; init; }
    public string? Series { get; init; }
    public IReadOnlyList<string> Tags { get; init; } = [];
    public BookFormatModel? Format { get; init; }
    public BookSortModel? SortBy { get; init; }
    public ulong Offset { get; init; }
    public ulong Limit { get; init; } = 50;
}

internal sealed class BookListItemModel
{
    public long BookId { get; init; }
    public string Title { get; init; } = string.Empty;
    public IReadOnlyList<string> Authors { get; init; } = [];
    public string Language { get; init; } = string.Empty;
    public string? Series { get; init; }
    public double? SeriesIndex { get; init; }
    public int? Year { get; init; }
    public IReadOnlyList<string> Tags { get; init; } = [];
    public BookFormatModel Format { get; init; }
    public string ManagedPath { get; init; } = string.Empty;
    public string? CoverPath { get; init; }
    public ulong SizeBytes { get; init; }
    public DateTimeOffset AddedAtUtc { get; init; }

    public string AuthorsText => Authors.Count == 0 ? "Unknown author" : string.Join(", ", Authors);
    public string TagsText => Tags.Count == 0 ? "No tags" : string.Join(", ", Tags);
    public string FormatText => Format.ToString().ToUpperInvariant();
}
