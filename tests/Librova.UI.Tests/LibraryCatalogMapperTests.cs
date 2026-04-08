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
                ManagedPath = @"Books\12\book.fb2"
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
                ManagedPath = @"Books\12\book.fb2",
                Sha256Hex = "abc"
            }));

        Assert.Contains("BookFormat", error.Message, StringComparison.Ordinal);
        Assert.Contains("book details format", error.Message, StringComparison.Ordinal);
    }
}
