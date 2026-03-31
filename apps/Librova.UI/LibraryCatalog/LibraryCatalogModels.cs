using System;
using System.Collections.Generic;
using Avalonia.Media;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using Avalonia;

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

internal sealed class BookListItemModel : INotifyPropertyChanged
{
    private IImage? _resolvedCoverImage;
    private IBrush? _coverBackgroundBrush;
    private string _coverPlaceholderText = "BOOK";
    private bool _isSelected;
    private IBrush? _cardBackgroundBrush;
    private IBrush? _cardBorderBrush;
    private Thickness _cardBorderThickness = new(1);

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
    public IImage? ResolvedCoverImage
    {
        get => _resolvedCoverImage;
        set => SetField(ref _resolvedCoverImage, value);
    }

    public IBrush? CoverBackgroundBrush
    {
        get => _coverBackgroundBrush;
        set => SetField(ref _coverBackgroundBrush, value);
    }

    public string CoverPlaceholderText
    {
        get => _coverPlaceholderText;
        set => SetField(ref _coverPlaceholderText, value);
    }

    public bool IsSelected
    {
        get => _isSelected;
        set => SetField(ref _isSelected, value);
    }

    public IBrush? CardBackgroundBrush
    {
        get => _cardBackgroundBrush;
        set => SetField(ref _cardBackgroundBrush, value);
    }

    public IBrush? CardBorderBrush
    {
        get => _cardBorderBrush;
        set => SetField(ref _cardBorderBrush, value);
    }

    public Thickness CardBorderThickness
    {
        get => _cardBorderThickness;
        set => SetField(ref _cardBorderThickness, value);
    }

    public string AuthorsText => Authors.Count == 0 ? "Unknown author" : string.Join(", ", Authors);
    public string TagsText => Tags.Count == 0 ? "No tags" : string.Join(", ", Tags);
    public string FormatText => Format.ToString().ToUpperInvariant();
    public bool HasResolvedCover => ResolvedCoverImage is not null;
    public bool ShowCoverPlaceholder => !HasResolvedCover;

    public event PropertyChangedEventHandler? PropertyChanged;

    private void SetField<T>(ref T field, T value, [CallerMemberName] string propertyName = "")
    {
        if (EqualityComparer<T>.Default.Equals(field, value))
        {
            return;
        }

        field = value;
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        if (propertyName is nameof(ResolvedCoverImage))
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(HasResolvedCover)));
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(ShowCoverPlaceholder)));
        }
    }
}

internal sealed class BookDetailsModel : INotifyPropertyChanged
{
    private IImage? _resolvedCoverImage;
    private IBrush? _coverBackgroundBrush;
    private string _coverPlaceholderText = "BOOK";

    public long BookId { get; init; }
    public string Title { get; init; } = string.Empty;
    public IReadOnlyList<string> Authors { get; init; } = [];
    public string Language { get; init; } = string.Empty;
    public string? Series { get; init; }
    public double? SeriesIndex { get; init; }
    public string? Publisher { get; init; }
    public int? Year { get; init; }
    public string? Isbn { get; init; }
    public IReadOnlyList<string> Tags { get; init; } = [];
    public string? Description { get; init; }
    public string? Identifier { get; init; }
    public BookFormatModel Format { get; init; }
    public string ManagedPath { get; init; } = string.Empty;
    public string? CoverPath { get; init; }
    public ulong SizeBytes { get; init; }
    public string Sha256Hex { get; init; } = string.Empty;
    public DateTimeOffset AddedAtUtc { get; init; }
    public IImage? ResolvedCoverImage
    {
        get => _resolvedCoverImage;
        set => SetField(ref _resolvedCoverImage, value);
    }

    public IBrush? CoverBackgroundBrush
    {
        get => _coverBackgroundBrush;
        set => SetField(ref _coverBackgroundBrush, value);
    }

    public string CoverPlaceholderText
    {
        get => _coverPlaceholderText;
        set => SetField(ref _coverPlaceholderText, value);
    }

    public bool HasResolvedCover => ResolvedCoverImage is not null;
    public bool ShowCoverPlaceholder => !HasResolvedCover;

    public event PropertyChangedEventHandler? PropertyChanged;

    private void SetField<T>(ref T field, T value, [CallerMemberName] string propertyName = "")
    {
        if (EqualityComparer<T>.Default.Equals(field, value))
        {
            return;
        }

        field = value;
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        if (propertyName is nameof(ResolvedCoverImage))
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(HasResolvedCover)));
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(ShowCoverPlaceholder)));
        }
    }
}
