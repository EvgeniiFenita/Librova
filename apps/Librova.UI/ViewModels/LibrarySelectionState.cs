using Avalonia.Media;
using Librova.UI.LibraryCatalog;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Librova.UI.ViewModels;

internal sealed class LibrarySelectionState
{
    private readonly Func<ulong, string> _formatSize;

    public LibrarySelectionState(Func<ulong, string> formatSize)
    {
        _formatSize = formatSize;
    }

    public BookListItemModel? SelectedBook { get; private set; }
    public BookDetailsModel? SelectedBookDetails { get; private set; }

    public string SelectedBookTitle => SelectedBookDetails?.Title ?? SelectedBook?.Title ?? "No book selected";

    public string SelectedBookAuthorText =>
        SelectedBookDetails is not null
            ? BuildAuthorsText(SelectedBookDetails.Authors)
            : SelectedBook?.AuthorsText ?? "Choose a book to inspect metadata.";

    public string SelectedBookAnnotationText =>
        string.IsNullOrWhiteSpace(SelectedBookDetails?.Description)
            ? "No annotation available."
            : SelectedBookDetails!.Description!;

    public string SelectedBookPathText =>
        SelectedBookDetails?.ManagedFileName
        ?? SelectedBook?.ManagedFileName
        ?? "No managed file available.";

    public IImage? SelectedBookCoverImage => SelectedBookDetails?.ResolvedCoverImage ?? SelectedBook?.ResolvedCoverImage;
    public IBrush? SelectedBookCoverBackgroundBrush => SelectedBookDetails?.CoverBackgroundBrush ?? SelectedBook?.CoverBackgroundBrush;
    public string SelectedBookCoverPlaceholderText => SelectedBookDetails?.CoverPlaceholderText ?? SelectedBook?.CoverPlaceholderText ?? "BOOK";
    public bool HasSelectedBookCover => SelectedBookCoverImage is not null;
    public bool ShowSelectedBookCoverPlaceholder => !HasSelectedBookCover;
    public BookFormatModel? SelectedBookFormat => SelectedBookDetails?.Format ?? SelectedBook?.Format;

    public IReadOnlyList<MetadataPair> SelectedBookMetadataPairs => BuildSelectedBookMetadataPairs();

    public void SetSelection(BookListItemModel? selectedBook)
    {
        SelectedBook = selectedBook;
        SelectedBookDetails = null;
    }

    public void SetDetails(BookDetailsModel? selectedBookDetails)
    {
        SelectedBookDetails = selectedBookDetails;
    }

    private IReadOnlyList<MetadataPair> BuildSelectedBookMetadataPairs()
    {
        if (SelectedBook is null)
        {
            return [];
        }

        var source = SelectedBookDetails;
        var pairs = new List<MetadataPair>();

        if (!string.IsNullOrWhiteSpace(source?.Language ?? SelectedBook.Language))
        {
            pairs.Add(new("Language", source?.Language ?? SelectedBook.Language));
        }

        if (source?.Year is not null || SelectedBook.Year is not null)
        {
            pairs.Add(new("Year", (source?.Year ?? SelectedBook.Year)!.Value.ToString()));
        }

        if (!string.IsNullOrWhiteSpace(source?.Publisher))
        {
            pairs.Add(new("Publisher", source!.Publisher!));
        }

        if (!string.IsNullOrWhiteSpace(source?.Series))
        {
            pairs.Add(new("Series", source!.SeriesIndex is null
                ? source.Series!
                : $"{source.Series} #{source.SeriesIndex:0.##}"));
        }
        else if (!string.IsNullOrWhiteSpace(SelectedBook.Series))
        {
            pairs.Add(new("Series", SelectedBook.SeriesIndex is null
                ? SelectedBook.Series!
                : $"{SelectedBook.Series} #{SelectedBook.SeriesIndex:0.##}"));
        }

        pairs.Add(new("Format", source?.Format.ToString().ToUpperInvariant() ?? SelectedBook.FormatText));

        var genres = source?.Genres.Count > 0
            ? string.Join(", ", source.Genres)
            : null;
        if (!string.IsNullOrWhiteSpace(genres))
        {
            pairs.Add(new("Genres", genres));
        }

        var tags = source?.Tags.Count > 0
            ? string.Join(", ", source.Tags)
            : SelectedBook.Tags.Count > 0
                ? SelectedBook.TagsText
                : null;
        if (!string.IsNullOrWhiteSpace(tags))
        {
            pairs.Add(new("Tags", tags));
        }

        var collections = source?.Collections.Count > 0
            ? string.Join(", ", source.Collections.Select(collection => collection.Name))
            : null;
        if (!string.IsNullOrWhiteSpace(collections))
        {
            pairs.Add(new("Collections", collections));
        }

        pairs.Add(new("Size", _formatSize(SelectedBook.SizeBytes)));
        return pairs;
    }

    private static string BuildAuthorsText(IReadOnlyList<string> authors) =>
        authors.Count == 0 ? "Unknown author" : string.Join(", ", authors);
}
