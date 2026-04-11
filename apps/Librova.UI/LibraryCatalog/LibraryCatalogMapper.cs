using Librova.V1;
using System;
using System.Linq;

namespace Librova.UI.LibraryCatalog;

internal static class LibraryCatalogMapper
{
    private static InvalidOperationException CreateContractViolationException(string context) =>
        new($"Received incomplete catalog transport payload while mapping {context}.");

    private static string? BuildCoverResourcePath(long bookId, string? extension)
    {
        if (string.IsNullOrWhiteSpace(extension))
        {
            return null;
        }

        var normalizedExtension = extension.StartsWith('.')
            ? extension[1..]
            : extension;
        return $"Covers/{bookId:0000000000}.{normalizedExtension}";
    }

    private static InvalidOperationException CreateUnexpectedEnumValueException<TEnum>(TEnum value, string context)
        where TEnum : struct, Enum =>
        new($"Received unexpected {typeof(TEnum).Name} value '{value}' while mapping {context}.");

    private static LibraryCatalogErrorCodeModel MapErrorCode(ErrorCode code) =>
        code switch
        {
            ErrorCode.Validation => LibraryCatalogErrorCodeModel.Validation,
            ErrorCode.UnsupportedFormat => LibraryCatalogErrorCodeModel.UnsupportedFormat,
            ErrorCode.DuplicateRejected => LibraryCatalogErrorCodeModel.DuplicateRejected,
            ErrorCode.DuplicateDecisionRequired => LibraryCatalogErrorCodeModel.DuplicateDecisionRequired,
            ErrorCode.ParserFailure => LibraryCatalogErrorCodeModel.ParserFailure,
            ErrorCode.ConverterUnavailable => LibraryCatalogErrorCodeModel.ConverterUnavailable,
            ErrorCode.ConverterFailed => LibraryCatalogErrorCodeModel.ConverterFailed,
            ErrorCode.StorageFailure => LibraryCatalogErrorCodeModel.StorageFailure,
            ErrorCode.DatabaseFailure => LibraryCatalogErrorCodeModel.DatabaseFailure,
            ErrorCode.Cancellation => LibraryCatalogErrorCodeModel.Cancellation,
            ErrorCode.IntegrityIssue => LibraryCatalogErrorCodeModel.IntegrityIssue,
            ErrorCode.NotFound => LibraryCatalogErrorCodeModel.NotFound,
            _ => throw CreateUnexpectedEnumValueException(code, "catalog domain error code")
        };

    private static string GetManagedFileName(BookListItem item)
    {
        if (!item.HasManagedFileName || string.IsNullOrWhiteSpace(item.ManagedFileName))
        {
            throw CreateContractViolationException("book list item managed file name");
        }

        return item.ManagedFileName;
    }

    private static string GetManagedFileName(BookDetails item)
    {
        if (!item.HasManagedFileName || string.IsNullOrWhiteSpace(item.ManagedFileName))
        {
            throw CreateContractViolationException("book details managed file name");
        }

        return item.ManagedFileName;
    }

    private static string? GetCoverRelativePath(long bookId, bool hasCoverResource, string? extension, string context)
    {
        if (!hasCoverResource)
        {
            return null;
        }

        var coverRelativePath = BuildCoverResourcePath(bookId, extension);
        if (coverRelativePath is null)
        {
            throw CreateContractViolationException(context);
        }

        return coverRelativePath;
    }

    private static LibraryCatalogDomainErrorModel MapDomainError(DomainError error) =>
        new()
        {
            Code = MapErrorCode(error.Code),
            Message = error.Message
        };

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

        if (!string.IsNullOrWhiteSpace(model.Genre))
        {
            request.Genre = model.Genre;
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
                _ => BookSort.Title
            };
        }

        request.SortDirection = model.SortDirection == BookSortDirectionModel.Descending
            ? BookSortDirection.Desc
            : BookSortDirection.Asc;

        return request;
    }

    public static BookListPageModel FromProto(ListBooksResponse response) =>
        new()
        {
            Items = response.Items.Select(FromProto).ToArray(),
            TotalCount = response.TotalCount,
            AvailableLanguages = response.AvailableLanguages.ToArray(),
            AvailableGenres = response.AvailableGenres.ToArray(),
            Statistics = response.Statistics is null ? null : FromProto(response.Statistics)
        };

    public static BookDetailsModel? FromProto(GetBookDetailsResponse response) =>
        response.Error is null
            ? response.Details is null ? null : FromProto(response.Details)
            : throw new LibraryCatalogDomainException(MapDomainError(response.Error));

    public static LibraryStatisticsModel FromProto(LibraryStatistics statistics) =>
        new()
        {
            BookCount = statistics.BookCount,
            TotalLibrarySizeBytes = statistics.TotalLibrarySizeBytes
        };

    public static string? FromProto(ExportBookResponse response) =>
        response.Error is null
            ? response.HasExportedPath ? response.ExportedPath : null
            : throw new LibraryCatalogDomainException(MapDomainError(response.Error));

    public static DeleteBookResultModel? FromProto(MoveBookToTrashResponse response) =>
        response.Error is not null
            ? throw new LibraryCatalogDomainException(MapDomainError(response.Error))
            : !response.HasTrashedBookId
                ? null
                : new DeleteBookResultModel
                {
                    BookId = response.TrashedBookId,
                    Destination = response.Destination switch
                    {
                        DeleteDestination.RecycleBin => DeleteDestinationModel.RecycleBin,
                        DeleteDestination.ManagedTrash => DeleteDestinationModel.ManagedTrash,
                        _ => throw CreateUnexpectedEnumValueException(response.Destination, "delete result destination")
                    },
                    HasOrphanedFiles = response.HasOrphanedFiles
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
                BookFormat.Epub => BookFormatModel.Epub,
                BookFormat.Fb2 => BookFormatModel.Fb2,
                _ => throw CreateUnexpectedEnumValueException(item.Format, "book list item format")
            },
            Storage = new BookStorageInfoModel
            {
                ManagedRelativePath = GetManagedFileName(item),
                CoverRelativePath = GetCoverRelativePath(
                    item.BookId,
                    item.HasCoverResourceAvailable && item.CoverResourceAvailable,
                    item.HasCoverFileExtension ? item.CoverFileExtension : null,
                    "book list item cover resource")
            },
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
                BookFormat.Epub => BookFormatModel.Epub,
                BookFormat.Fb2 => BookFormatModel.Fb2,
                _ => throw CreateUnexpectedEnumValueException(item.Format, "book details format")
            },
            Storage = new BookStorageInfoModel
            {
                ManagedRelativePath = GetManagedFileName(item),
                CoverRelativePath = GetCoverRelativePath(
                    item.BookId,
                    item.HasCoverResourceAvailable && item.CoverResourceAvailable,
                    item.HasCoverFileExtension ? item.CoverFileExtension : null,
                    "book details cover resource"),
                HasContentHash = item.HasContentHashAvailable && item.ContentHashAvailable
            },
            SizeBytes = item.SizeBytes,
            AddedAtUtc = DateTimeOffset.FromUnixTimeMilliseconds(item.AddedAtUnixMs)
        };
}
