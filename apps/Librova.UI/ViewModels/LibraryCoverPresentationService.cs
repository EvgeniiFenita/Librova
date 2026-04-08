using Avalonia;
using Avalonia.Media;
using Librova.UI.LibraryCatalog;
using Librova.UI.Logging;
using System;
using System.IO;
using System.Linq;

namespace Librova.UI.ViewModels;

internal sealed class LibraryCoverPresentationService
{
    private static readonly IBrush RealCoverBackground = new SolidColorBrush(Color.Parse("#0A0700"));
    private readonly ICoverImageLoader _coverImageLoader;
    private readonly string _libraryRoot;

    public LibraryCoverPresentationService(string? libraryRoot = null, ICoverImageLoader? coverImageLoader = null)
    {
        _libraryRoot = libraryRoot ?? string.Empty;
        _coverImageLoader = coverImageLoader ?? new FileCoverImageLoader();
    }

    public BookListItemModel Prepare(BookListItemModel item)
    {
        item.ResolvedCoverImage = LoadCoverImage(item.CoverPath);
        item.CoverBackgroundBrush = CreateCoverBackgroundBrush(item.ResolvedCoverImage, item.BookId, item.Title, item.AuthorsText);
        item.CoverPlaceholderText = BuildCoverPlaceholderText(item.Title);
        return item;
    }

    public BookDetailsModel Prepare(BookDetailsModel item)
    {
        item.ResolvedCoverImage = LoadCoverImage(item.CoverPath);
        item.CoverBackgroundBrush = CreateCoverBackgroundBrush(
            item.ResolvedCoverImage,
            item.BookId,
            item.Title,
            item.Authors.Count == 0 ? "Unknown author" : string.Join(", ", item.Authors));
        item.CoverPlaceholderText = BuildCoverPlaceholderText(item.Title);
        return item;
    }

    private IImage? LoadCoverImage(string? coverPath)
    {
        if (string.IsNullOrWhiteSpace(coverPath))
        {
            return null;
        }

        try
        {
            var resolvedPath = ResolveExistingCoverPathWithinLibraryRoot(coverPath, _libraryRoot);
            if (string.IsNullOrWhiteSpace(resolvedPath))
            {
                return null;
            }

            return _coverImageLoader.Load(resolvedPath);
        }
        catch (Exception error)
        {
            UiLogging.Error(
                error,
                "Failed to load managed cover image. CoverPath={CoverPath} LibraryRoot={LibraryRoot}",
                coverPath,
                _libraryRoot);
            return null;
        }
    }

    internal static string? ResolveExistingCoverPathWithinLibraryRoot(string coverPath, string libraryRoot)
    {
        if (string.IsNullOrWhiteSpace(libraryRoot))
        {
            return null;
        }

        var candidatePath = Path.IsPathFullyQualified(coverPath)
            ? Path.GetFullPath(coverPath)
            : Path.GetFullPath(Path.Combine(libraryRoot, coverPath));
        if (!File.Exists(candidatePath))
        {
            return null;
        }

        var canonicalRoot = CanonicalizeExistingPath(libraryRoot);
        var canonicalCandidate = CanonicalizeExistingPath(candidatePath);
        if (!IsPathWithinRoot(canonicalRoot, canonicalCandidate))
        {
            UiLogging.Warning(
                "Rejected cover path that does not resolve within the library root. Path={Path} LibraryRoot={LibraryRoot}",
                canonicalCandidate,
                canonicalRoot);
            return null;
        }

        return canonicalCandidate;
    }

    private static string CanonicalizeExistingPath(string path)
    {
        var fullPath = Path.GetFullPath(path);
        var root = Path.GetPathRoot(fullPath);
        if (string.IsNullOrWhiteSpace(root))
        {
            throw new IOException("Path does not contain a root.");
        }

        var relativePath = Path.GetRelativePath(root, fullPath);
        var current = root;

        if (string.Equals(relativePath, ".", StringComparison.Ordinal))
        {
            return Path.GetFullPath(current);
        }

        foreach (var segment in relativePath.Split([Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar], StringSplitOptions.RemoveEmptyEntries))
        {
            var candidate = Path.Combine(current, segment);
            current = ResolveExistingPathSegment(candidate);
        }

        return Path.GetFullPath(current);
    }

    private static string ResolveExistingPathSegment(string path)
    {
        if (Directory.Exists(path))
        {
            var directory = new DirectoryInfo(path);
            var resolved = directory.ResolveLinkTarget(returnFinalTarget: true);
            return Path.GetFullPath(resolved?.FullName ?? directory.FullName);
        }

        if (File.Exists(path))
        {
            var file = new FileInfo(path);
            var resolved = file.ResolveLinkTarget(returnFinalTarget: true);
            return Path.GetFullPath(resolved?.FullName ?? file.FullName);
        }

        throw new IOException("Path segment does not exist.");
    }

    private static bool IsPathWithinRoot(string rootPath, string candidatePath)
    {
        var relativePath = Path.GetRelativePath(rootPath, candidatePath);
        return string.Equals(relativePath, ".", StringComparison.Ordinal)
            || (!relativePath.Equals("..", StringComparison.Ordinal)
                && !relativePath.StartsWith($"..{Path.DirectorySeparatorChar}", StringComparison.Ordinal)
                && !relativePath.StartsWith($"..{Path.AltDirectorySeparatorChar}", StringComparison.Ordinal)
                && !Path.IsPathRooted(relativePath));
    }

    private static string BuildCoverPlaceholderText(string title)
    {
        if (string.IsNullOrWhiteSpace(title))
        {
            return "BOOK";
        }

        var letter = title.Trim().FirstOrDefault(character => char.IsLetterOrDigit(character));
        return letter == default ? "BOOK" : char.ToUpperInvariant(letter).ToString();
    }

    private static IBrush CreateCoverBackgroundBrush(IImage? resolvedCoverImage, long bookId, string title, string authorsText) =>
        resolvedCoverImage is null
            ? CreateGradientBrush(bookId, title, authorsText)
            : RealCoverBackground;

    private static IBrush CreateGradientBrush(long bookId, string title, string authorsText)
    {
        var palettes = new[]
        {
            (Start: Color.Parse("#6C4B3A"), End: Color.Parse("#D9A066")),
            (Start: Color.Parse("#5C3A28"), End: Color.Parse("#C4855A")),
            (Start: Color.Parse("#5A2F3D"), End: Color.Parse("#C18E78")),
            (Start: Color.Parse("#40523B"), End: Color.Parse("#B6C58E")),
            (Start: Color.Parse("#6B4C1E"), End: Color.Parse("#D2B87A"))
        };

        var hash = HashCode.Combine(bookId, title, authorsText);
        var index = SelectPaletteIndex(hash, palettes.Length);
        var palette = palettes[index];
        return new LinearGradientBrush
        {
            StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
            EndPoint = new RelativePoint(1, 1, RelativeUnit.Relative),
            GradientStops =
            [
                new GradientStop(palette.Start, 0),
                new GradientStop(palette.End, 1)
            ]
        };
    }

    internal static int SelectPaletteIndex(int hash, int paletteCount)
    {
        if (paletteCount <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(paletteCount));
        }

        return (int)((uint)hash % (uint)paletteCount);
    }
}

internal interface ICoverImageLoader
{
    IImage? Load(string absolutePath);
}

internal sealed class FileCoverImageLoader : ICoverImageLoader
{
    public IImage? Load(string absolutePath)
    {
        using var stream = File.OpenRead(absolutePath);
        return new Avalonia.Media.Imaging.Bitmap(stream);
    }
}
