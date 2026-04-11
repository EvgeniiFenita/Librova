using Librova.UI.Desktop;
using Librova.UI.LibraryCatalog;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class LibrarySelectionWorkflowController
{
    private static readonly TimeSpan DetailsTransportTimeout = TimeSpan.FromSeconds(5);
    private static readonly TimeSpan ExportTransportTimeout = TimeSpan.FromSeconds(10);
    private static readonly TimeSpan ExportAsEpubTransportTimeout = TimeSpan.FromMinutes(2);
    private static readonly TimeSpan DeleteTransportTimeout = TimeSpan.FromSeconds(5);

    private readonly ILibraryCatalogService _libraryCatalogService;
    private readonly IPathSelectionService _pathSelectionService;

    public LibrarySelectionWorkflowController(
        ILibraryCatalogService libraryCatalogService,
        IPathSelectionService pathSelectionService)
    {
        _libraryCatalogService = libraryCatalogService;
        _pathSelectionService = pathSelectionService;
    }

    public LibrarySelectionToggleResult ToggleSelection(
        BookListItemModel? currentSelection,
        BookListItemModel selectedBook)
    {
        if (currentSelection?.BookId == selectedBook.BookId)
        {
            return new(null, "Closed book details.", ShouldLoadDetails: false);
        }

        return new(selectedBook, $"Selected '{selectedBook.Title}'.", ShouldLoadDetails: true);
    }

    public async Task<LibrarySelectionDetailsResult> LoadDetailsAsync(
        BookListItemModel selectedBook,
        Func<BookDetailsModel, BookDetailsModel> prepare,
        CancellationToken cancellationToken)
    {
        try
        {
            var details = await _libraryCatalogService.GetBookDetailsAsync(
                selectedBook.BookId,
                DetailsTransportTimeout,
                cancellationToken);

            if (details is null)
            {
                return new(null, "Book details were not found.");
            }

            var preparedDetails = prepare(details);
            return new(preparedDetails, $"Loaded details for '{preparedDetails.Title}'.");
        }
        catch (LibraryCatalogDomainException error)
        {
            return new(
                null,
                error.Error.Code is LibraryCatalogErrorCodeModel.NotFound
                    ? "Book details were not found."
                    : error.Message);
        }
    }

    public async Task<LibrarySelectionExportPlan> PrepareExportAsync(
        BookListItemModel selectedBook,
        BookDetailsModel? selectedBookDetails,
        BookFormatModel? selectedBookFormat,
        BookFormatModel? exportFormat,
        CancellationToken cancellationToken)
    {
        var suggestedFileName = BuildSuggestedExportFileName(selectedBook, selectedBookDetails, selectedBookFormat, exportFormat);
        var destinationPath = await _pathSelectionService.PickExportDestinationAsync(suggestedFileName, cancellationToken);
        if (string.IsNullOrWhiteSpace(destinationPath))
        {
            return new(null, "Export was cancelled before a destination was chosen.");
        }

        var statusText = exportFormat is BookFormatModel.Epub
            ? $"Exporting '{TruncateTitle(selectedBook.Title)}' as EPUB..."
            : $"Exporting '{TruncateTitle(selectedBook.Title)}'...";
        var operationTimeout = exportFormat is BookFormatModel.Epub
            ? TimeSpan.FromMinutes(3)
            : TimeSpan.FromSeconds(30);
        var transportTimeout = exportFormat is BookFormatModel.Epub
            ? ExportAsEpubTransportTimeout
            : ExportTransportTimeout;

        return new(destinationPath, statusText, operationTimeout, transportTimeout);
    }

    public async Task<string> ExportAsync(
        BookListItemModel selectedBook,
        string destinationPath,
        BookFormatModel? exportFormat,
        TimeSpan transportTimeout,
        CancellationToken cancellationToken)
    {
        try
        {
            var exportedPath = await _libraryCatalogService.ExportBookAsync(
                selectedBook.BookId,
                destinationPath,
                exportFormat,
                transportTimeout,
                cancellationToken);

            return string.IsNullOrWhiteSpace(exportedPath)
                ? "Selected book could not be exported."
                : exportFormat is BookFormatModel.Epub
                    ? $"Exported '{selectedBook.Title}' as EPUB to '{exportedPath}'."
                    : $"Exported '{selectedBook.Title}' to '{exportedPath}'.";
        }
        catch (LibraryCatalogDomainException error)
        {
            return error.Error.Code is LibraryCatalogErrorCodeModel.NotFound
                ? "Selected book could not be exported."
                : error.Message;
        }
    }

    public async Task<string> MoveSelectedBookToTrashAsync(
        BookListItemModel selectedBook,
        Func<Task> refreshLoadedRangeAsync,
        CancellationToken cancellationToken)
    {
        try
        {
            var deleteResult = await _libraryCatalogService.MoveBookToTrashAsync(
                selectedBook.BookId,
                DeleteTransportTimeout,
                cancellationToken);

            if (deleteResult is null)
            {
                return "Selected book could not be moved to Recycle Bin.";
            }

            await refreshLoadedRangeAsync();
            return GetDeleteStatusText(selectedBook.Title, deleteResult);
        }
        catch (LibraryCatalogDomainException error)
        {
            return error.Error.Code is LibraryCatalogErrorCodeModel.NotFound
                ? "Selected book could not be moved to Recycle Bin."
                : error.Message;
        }
    }

    private static string GetDeleteStatusText(string deletedTitle, DeleteBookResultModel deleteResult)
    {
        if (deleteResult.HasOrphanedFiles)
        {
            return deleteResult.Destination == DeleteDestinationModel.RecycleBin
                ? $"Removed '{deletedTitle}' from the library, moved the available managed files to Recycle Bin, and left some managed files on disk."
                : $"Removed '{deletedTitle}' from the library, but some managed files could not be moved and were left on disk.";
        }

        return deleteResult.Destination == DeleteDestinationModel.RecycleBin
            ? $"Moved '{deletedTitle}' to Recycle Bin."
            : $"Moved '{deletedTitle}' to library Trash because Windows Recycle Bin was unavailable.";
    }

    private static string BuildSuggestedExportFileName(
        BookListItemModel selectedBook,
        BookDetailsModel? selectedBookDetails,
        BookFormatModel? selectedBookFormat,
        BookFormatModel? exportFormat)
    {
        const string fallbackBaseName = "book";

        var title = SanitizeFileNamePart(selectedBookDetails?.Title ?? selectedBook.Title);
        var authors = SanitizeFileNamePart(
            selectedBookDetails is not null
                ? BuildAuthorsText(selectedBookDetails.Authors)
                : selectedBook.AuthorsText);

        var baseName = !string.IsNullOrWhiteSpace(title) && !string.IsNullOrWhiteSpace(authors)
            ? $"{title} - {authors}"
            : !string.IsNullOrWhiteSpace(title)
                ? title
                : !string.IsNullOrWhiteSpace(authors)
                    ? authors
                    : fallbackBaseName;

        var extension = exportFormat switch
        {
            BookFormatModel.Fb2 => ".fb2",
            BookFormatModel.Epub => ".epub",
            _ => selectedBookFormat switch
            {
                BookFormatModel.Fb2 => ".fb2",
                BookFormatModel.Epub => ".epub",
                _ => null
            }
        };
        if (string.IsNullOrWhiteSpace(extension))
        {
            extension = ".epub";
        }

        return baseName + extension.ToLowerInvariant();
    }

    private static string SanitizeFileNamePart(string? value)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            return string.Empty;
        }

        var builder = new StringBuilder(value.Length);
        var invalidCharacters = Path.GetInvalidFileNameChars();
        var previousWasWhitespace = false;

        foreach (var character in value.Trim())
        {
            if (Array.IndexOf(invalidCharacters, character) >= 0 || char.IsControl(character))
            {
                if (!previousWasWhitespace && builder.Length > 0)
                {
                    builder.Append(' ');
                    previousWasWhitespace = true;
                }

                continue;
            }

            if (char.IsWhiteSpace(character))
            {
                if (!previousWasWhitespace && builder.Length > 0)
                {
                    builder.Append(' ');
                    previousWasWhitespace = true;
                }

                continue;
            }

            builder.Append(character);
            previousWasWhitespace = false;
        }

        return builder.ToString().Trim(' ', '.');
    }

    private static string TruncateTitle(string? title, int maxLength = 40)
    {
        if (string.IsNullOrEmpty(title) || title.Length <= maxLength)
        {
            return title ?? string.Empty;
        }

        return string.Concat(title.AsSpan(0, maxLength), "…");
    }

    private static string BuildAuthorsText(IReadOnlyList<string> authors) =>
        authors.Count == 0 ? "Unknown author" : string.Join(", ", authors);
}

internal sealed record LibrarySelectionToggleResult(
    BookListItemModel? SelectedBook,
    string StatusText,
    bool ShouldLoadDetails);

internal sealed record LibrarySelectionDetailsResult(
    BookDetailsModel? Details,
    string StatusText);

internal sealed record LibrarySelectionExportPlan(
    string? DestinationPath,
    string StatusText,
    TimeSpan OperationTimeout = default,
    TimeSpan TransportTimeout = default)
{
    public bool ShouldExport => !string.IsNullOrWhiteSpace(DestinationPath);
}
