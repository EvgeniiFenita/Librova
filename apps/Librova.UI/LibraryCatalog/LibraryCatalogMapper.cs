using Librova.V1;
using System;
using System.Linq;

namespace Librova.UI.LibraryCatalog;

internal static class LibraryCatalogMapper
{
    public static BookListRequest ToProto(BookListRequestModel model)
    {
        var request = new BookListRequest
        {
            Text = model.Text,
            Offset = model.Offset,
            Limit = model.Limit
        };

        if (!string.IsNullOrWhiteSpace(model.Author))
        {
            request.Author = model.Author;
        }

        if (!string.IsNullOrWhiteSpace(model.Language))
        {
            request.Language = model.Language;
        }

        if (!string.IsNullOrWhiteSpace(model.Series))
        {
            request.Series = model.Series;
        }

        request.Tags.AddRange(model.Tags);

        if (model.Format is not null)
        {
            request.Format = model.Format.Value switch
            {
                BookFormatModel.Fb2 => BookFormat.Fb2,
                _ => BookFormat.Epub
            };
        }

        if (model.SortBy is not null)
        {
            request.SortBy = model.SortBy.Value switch
            {
                BookSortModel.Author => BookSort.Author,
                BookSortModel.DateAdded => BookSort.DateAdded,
                BookSortModel.Series => BookSort.Series,
                BookSortModel.Year => BookSort.Year,
                BookSortModel.FileSize => BookSort.FileSize,
                _ => BookSort.Title
            };
        }

        return request;
    }

    public static IReadOnlyList<BookListItemModel> FromProto(ListBooksResponse response) =>
        response.Items.Select(FromProto).ToArray();

    public static BookListItemModel FromProto(BookListItem item) =>
        new()
        {
            BookId = item.BookId,
            Title = item.Title,
            Authors = item.Authors.ToArray(),
            Language = item.Language,
            Series = item.HasSeries ? item.Series : null,
            SeriesIndex = item.HasSeriesIndex ? item.SeriesIndex : null,
            Year = item.HasYear ? item.Year : null,
            Tags = item.Tags.ToArray(),
            Format = item.Format switch
            {
                BookFormat.Fb2 => BookFormatModel.Fb2,
                _ => BookFormatModel.Epub
            },
            ManagedPath = item.ManagedPath,
            CoverPath = item.HasCoverPath ? item.CoverPath : null,
            SizeBytes = item.SizeBytes,
            AddedAtUtc = DateTimeOffset.FromUnixTimeMilliseconds(item.AddedAtUnixMs)
        };
}
