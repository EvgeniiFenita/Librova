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

    public static BookListPageModel FromProto(ListBooksResponse response) =>
        new()
        {
            Items = response.Items.Select(FromProto).ToArray(),
            TotalCount = response.TotalCount,
            AvailableLanguages = response.AvailableLanguages.ToArray()
        };

    public static BookDetailsModel? FromProto(GetBookDetailsResponse response) =>
        response.Details is null ? null : FromProto(response.Details);

    public static LibraryStatisticsModel FromProto(GetLibraryStatisticsResponse response) =>
        new()
        {
            BookCount = response.Statistics?.BookCount ?? 0,
            TotalManagedBookSizeBytes = response.Statistics?.TotalManagedBookSizeBytes ?? 0
        };

    public static string? FromProto(ExportBookResponse response) =>
        response.HasExportedPath ? response.ExportedPath : null;

    public static DeleteBookResultModel? FromProto(MoveBookToTrashResponse response) =>
        !response.HasTrashedBookId
            ? null
            : new DeleteBookResultModel
            {
                BookId = response.TrashedBookId,
                Destination = response.Destination switch
                {
                    DeleteDestination.ManagedTrash => DeleteDestinationModel.ManagedTrash,
                    _ => DeleteDestinationModel.RecycleBin
                }
            };

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

    public static BookDetailsModel FromProto(BookDetails item) =>
        new()
        {
            BookId = item.BookId,
            Title = item.Title,
            Authors = item.Authors.ToArray(),
            Language = item.Language,
            Series = item.HasSeries ? item.Series : null,
            SeriesIndex = item.HasSeriesIndex ? item.SeriesIndex : null,
            Publisher = item.HasPublisher ? item.Publisher : null,
            Year = item.HasYear ? item.Year : null,
            Isbn = item.HasIsbn ? item.Isbn : null,
            Tags = item.Tags.ToArray(),
            Description = item.HasDescription ? item.Description : null,
            Identifier = item.HasIdentifier ? item.Identifier : null,
            Format = item.Format switch
            {
                BookFormat.Fb2 => BookFormatModel.Fb2,
                _ => BookFormatModel.Epub
            },
            ManagedPath = item.ManagedPath,
            CoverPath = item.HasCoverPath ? item.CoverPath : null,
            SizeBytes = item.SizeBytes,
            Sha256Hex = item.Sha256Hex,
            AddedAtUtc = DateTimeOffset.FromUnixTimeMilliseconds(item.AddedAtUnixMs)
        };
}
