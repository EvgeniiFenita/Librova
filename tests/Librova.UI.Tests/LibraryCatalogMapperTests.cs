using Librova.UI.LibraryCatalog;
using Librova.V1;
using System;
using Xunit;

namespace Librova.UI.Tests;

public sealed class LibraryCatalogMapperTests
{
    [Fact]
    public void Mapper_RejectsUnknownDeleteDestination()
    {
        var error = Assert.Throws<InvalidOperationException>(() =>
            LibraryCatalogMapper.FromProto(new MoveBookToTrashResponse
            {
                TrashedBookId = 12,
                Destination = (DeleteDestination)999
            }));

        Assert.Contains("DeleteDestination", error.Message, StringComparison.Ordinal);
        Assert.Contains("delete result destination", error.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void Mapper_RejectsUnknownBookFormatInListItems()
    {
        var error = Assert.Throws<InvalidOperationException>(() =>
            LibraryCatalogMapper.FromProto(new BookListItem
            {
                BookId = 12,
                Title = "Book",
                Language = "en",
                Format = (BookFormat)999,
                ManagedFileName = "book.fb2"
            }));

        Assert.Contains("BookFormat", error.Message, StringComparison.Ordinal);
        Assert.Contains("book list item format", error.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void Mapper_RejectsUnknownBookFormatInDetails()
    {
        var error = Assert.Throws<InvalidOperationException>(() =>
            LibraryCatalogMapper.FromProto(new BookDetails
            {
                BookId = 12,
                Title = "Book",
                Language = "en",
                Format = (BookFormat)999,
                ManagedFileName = "book.fb2"
            }));

        Assert.Contains("BookFormat", error.Message, StringComparison.Ordinal);
        Assert.Contains("book details format", error.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void Mapper_ThrowsStructuredDomainExceptionForDetailsError()
    {
        var error = Assert.Throws<LibraryCatalogDomainException>(() =>
            LibraryCatalogMapper.FromProto(new GetBookDetailsResponse
            {
                Error = new DomainError
                {
                    Code = ErrorCode.NotFound,
                    Message = "Book 42 was not found."
                }
            }));

        Assert.Equal(LibraryCatalogErrorCodeModel.NotFound, error.Error.Code);
        Assert.Equal("Book 42 was not found.", error.Message);
    }

    [Fact]
    public void Mapper_ThrowsStructuredDomainExceptionForExportError()
    {
        var error = Assert.Throws<LibraryCatalogDomainException>(() =>
            LibraryCatalogMapper.FromProto(new ExportBookResponse
            {
                Error = new DomainError
                {
                    Code = ErrorCode.NotFound,
                    Message = "Book 42 was not found for export."
                }
            }));

        Assert.Equal(LibraryCatalogErrorCodeModel.NotFound, error.Error.Code);
        Assert.Equal("Book 42 was not found for export.", error.Message);
    }

    [Fact]
    public void Mapper_ThrowsStructuredDomainExceptionForTrashError()
    {
        var error = Assert.Throws<LibraryCatalogDomainException>(() =>
            LibraryCatalogMapper.FromProto(new MoveBookToTrashResponse
            {
                Error = new DomainError
                {
                    Code = ErrorCode.NotFound,
                    Message = "Book 42 was not found for deletion."
                }
            }));

        Assert.Equal(LibraryCatalogErrorCodeModel.NotFound, error.Error.Code);
        Assert.Equal("Book 42 was not found for deletion.", error.Message);
    }

    [Fact]
    public void Mapper_MapsStorageFieldsIntoDedicatedStorageMetadata()
    {
        var listItem = LibraryCatalogMapper.FromProto(new BookListItem
        {
            BookId = 12,
            Title = "Roadside Picnic",
            Language = "en",
            Format = BookFormat.Epub,
            ManagedFileName = "safe-roadside.epub",
            CoverResourceAvailable = true,
            CoverRelativePath = "Objects/8a/5b/0000000012.cover.jpg"
        });

        var details = LibraryCatalogMapper.FromProto(new BookDetails
        {
            BookId = 12,
            Title = "Roadside Picnic",
            Language = "en",
            Format = BookFormat.Epub,
            ManagedFileName = "safe-roadside.epub",
            CoverResourceAvailable = true,
            ContentHashAvailable = true,
            CoverRelativePath = "Objects/8a/5b/0000000012.cover.jpg"
        });

        Assert.Equal("safe-roadside.epub", listItem.Storage.ManagedRelativePath);
        Assert.Equal("safe-roadside.epub", listItem.ManagedFileName);
        Assert.True(listItem.HasCoverResource);

        Assert.Equal("safe-roadside.epub", details.Storage.ManagedRelativePath);
        Assert.Equal("Objects/8a/5b/0000000012.cover.jpg", details.Storage.CoverRelativePath);
        Assert.True(details.HasContentHash);
        Assert.Equal("safe-roadside.epub", details.ManagedFileName);
    }

    [Fact]
    public void Mapper_UsesTransportOwnedCoverRelativePath()
    {
        var details = LibraryCatalogMapper.FromProto(new BookDetails
        {
            BookId = 123456,
            Title = "Bucketed",
            Language = "en",
            Format = BookFormat.Epub,
            ManagedFileName = "bucketed.epub",
            CoverResourceAvailable = true,
            CoverRelativePath = "Objects/custom/cover-path.png"
        });

        Assert.Equal("Objects/custom/cover-path.png", details.Storage.CoverRelativePath);
    }

    [Fact]
    public void Mapper_MapsAvailableGenresFromListResponse()
    {
        var page = LibraryCatalogMapper.FromProto(new ListBooksResponse
        {
            TotalCount = 2,
            AvailableLanguages = { "en" },
            AvailableGenres = { "adventure", "sci-fi" }
        });

        Assert.Equal(["adventure", "sci-fi"], page.AvailableGenres);
        Assert.Equal(["en"], page.AvailableLanguages);
    }

    [Fact]
    public void Mapper_RejectsMissingManagedFileNameInListItems()
    {
        var error = Assert.Throws<InvalidOperationException>(() =>
            LibraryCatalogMapper.FromProto(new BookListItem
            {
                BookId = 12,
                Title = "Book",
                Language = "en",
                Format = BookFormat.Epub
            }));

        Assert.Contains("incomplete catalog transport payload", error.Message, StringComparison.Ordinal);
        Assert.Contains("book list item managed file name", error.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void Mapper_RejectsMissingManagedFileNameInDetails()
    {
        var error = Assert.Throws<InvalidOperationException>(() =>
            LibraryCatalogMapper.FromProto(new BookDetails
            {
                BookId = 12,
                Title = "Book",
                Language = "en",
                Format = BookFormat.Epub
            }));

        Assert.Contains("incomplete catalog transport payload", error.Message, StringComparison.Ordinal);
        Assert.Contains("book details managed file name", error.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void Mapper_RejectsUnknownDomainErrorCodeInDetailsResponse()
    {
        var error = Assert.Throws<InvalidOperationException>(() =>
            LibraryCatalogMapper.FromProto(new GetBookDetailsResponse
            {
                Error = new DomainError
                {
                    Code = (ErrorCode)999,
                    Message = "drift"
                }
            }));

        Assert.Contains("ErrorCode", error.Message, StringComparison.Ordinal);
        Assert.Contains("catalog domain error code", error.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void Mapper_RejectsUnknownDomainErrorCodeInExportResponse()
    {
        var error = Assert.Throws<InvalidOperationException>(() =>
            LibraryCatalogMapper.FromProto(new ExportBookResponse
            {
                Error = new DomainError
                {
                    Code = (ErrorCode)999,
                    Message = "drift"
                }
            }));

        Assert.Contains("ErrorCode", error.Message, StringComparison.Ordinal);
        Assert.Contains("catalog domain error code", error.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void Mapper_RejectsUnknownDomainErrorCodeInTrashResponse()
    {
        var error = Assert.Throws<InvalidOperationException>(() =>
            LibraryCatalogMapper.FromProto(new MoveBookToTrashResponse
            {
                Error = new DomainError
                {
                    Code = (ErrorCode)999,
                    Message = "drift"
                }
            }));

        Assert.Contains("ErrorCode", error.Message, StringComparison.Ordinal);
        Assert.Contains("catalog domain error code", error.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void Mapper_MapsLanguagesAndGenresToProto()
    {
        var request = new BookListRequestModel
        {
            Languages = ["en", "ru"],
            Genres = ["classic", "sci-fi"],
            Limit = 20
        };

        var proto = LibraryCatalogMapper.ToProto(request);

        Assert.Equal(["en", "ru"], proto.Languages);
        Assert.Equal(["classic", "sci-fi"], proto.Genres);
    }

    [Fact]
    public void Mapper_MapsEmptyLanguagesAndGenresToEmptyProtoFields()
    {
        var request = new BookListRequestModel { Limit = 10 };

        var proto = LibraryCatalogMapper.ToProto(request);

        Assert.Empty(proto.Languages);
        Assert.Empty(proto.Genres);
    }
}
