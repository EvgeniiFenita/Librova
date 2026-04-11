using System;
using System.Collections.Generic;
using System.Linq;
using System.IO;
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

internal enum BookSortDirectionModel
{
    Ascending,
    Descending
}

internal enum BookSortModel
{
    Title,
    Author,
    DateAdded
}

internal sealed record SortKeyOption(BookSortModel Key, string Label)
{
    public static IReadOnlyList<SortKeyOption> All { get; } =
    [
        new(BookSortModel.Title, "Title"),
        new(BookSortModel.Author, "Author"),
        new(BookSortModel.DateAdded, "Date Added"),
    ];

    public static SortKeyOption Default { get; } = All[0];

    public static SortKeyOption For(BookSortModel key) =>
        All.FirstOrDefault(o => o.Key == key) ?? Default;
}

internal sealed class BookListRequestModel
{
    public string Text { get; init; } = string.Empty;
    public string? Author { get; init; }
    public string? Language { get; init; }
    public string? Genre { get; init; }
    public string? Series { get; init; }
    public IReadOnlyList<string> Tags { get; init; } = [];
    public BookFormatModel? Format { get; init; }
    public BookSortModel? SortBy { get; init; }
    public BookSortDirectionModel SortDirection { get; init; } = BookSortDirectionModel.Ascending;
    public ulong Offset { get; init; }
    public ulong Limit { get; init; } = 50;
}

internal sealed class BookListPageModel
{
    public IReadOnlyList<BookListItemModel> Items { get; init; } = [];
    public ulong TotalCount { get; init; }
    public IReadOnlyList<string> AvailableLanguages { get; init; } = [];
    public IReadOnlyList<string> AvailableGenres { get; init; } = [];
    public LibraryStatisticsModel? Statistics { get; init; } = new();
}

internal sealed record BookStorageInfoModel
{
    public string ManagedRelativePath { get; init; } = string.Empty;
    public string? CoverRelativePath { get; init; }

    public string ManagedFileName =>
        string.IsNullOrWhiteSpace(ManagedRelativePath)
            ? string.Empty
            : Path.GetFileName(ManagedRelativePath.Replace('/', Path.DirectorySeparatorChar));

    public bool HasCoverResource => !string.IsNullOrWhiteSpace(CoverRelativePath);
    public bool HasContentHash { get; init; }
}

internal sealed record BookListItemDataModel
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
    public BookStorageInfoModel Storage { get; init; } = new();
    public ulong SizeBytes { get; init; }
    public DateTimeOffset AddedAtUtc { get; init; }

    public string ManagedPath
    {
        get => Storage.ManagedRelativePath;
        init => Storage = Storage with { ManagedRelativePath = value };
    }

    public string? CoverPath
    {
        get => Storage.CoverRelativePath;
        init => Storage = Storage with { CoverRelativePath = value };
    }
}

internal sealed class BookCoverPresentationModel : INotifyPropertyChanged
{
    private IImage? _resolvedCoverImage;
    private IBrush? _coverBackgroundBrush;
    private string _coverPlaceholderText = "BOOK";

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

internal sealed class BookCardPresentationModel : INotifyPropertyChanged
{
    private bool _isSelected;
    private IBrush? _cardBackgroundBrush;
    private IBrush? _cardBorderBrush;
    private Thickness _cardBorderThickness = new(0);

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

    public event PropertyChangedEventHandler? PropertyChanged;

    private void SetField<T>(ref T field, T value, [CallerMemberName] string propertyName = "")
    {
        if (EqualityComparer<T>.Default.Equals(field, value))
        {
            return;
        }

        field = value;
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
}

internal sealed class BookListItemModel : INotifyPropertyChanged
{
    private BookListItemDataModel _data = new();

    public BookListItemDataModel Data
    {
        get => _data;
        init => _data = value ?? new BookListItemDataModel();
    }

    public BookCoverPresentationModel CoverPresentation { get; init; } = new();
    public BookCardPresentationModel CardPresentation { get; init; } = new();

    public long BookId
    {
        get => Data.BookId;
        init => _data = Data with { BookId = value };
    }

    public string Title
    {
        get => Data.Title;
        init => _data = Data with { Title = value };
    }

    public IReadOnlyList<string> Authors
    {
        get => Data.Authors;
        init => _data = Data with { Authors = value };
    }

    public string Language
    {
        get => Data.Language;
        init => _data = Data with { Language = value };
    }

    public string? Series
    {
        get => Data.Series;
        init => _data = Data with { Series = value };
    }

    public double? SeriesIndex
    {
        get => Data.SeriesIndex;
        init => _data = Data with { SeriesIndex = value };
    }

    public int? Year
    {
        get => Data.Year;
        init => _data = Data with { Year = value };
    }

    public IReadOnlyList<string> Tags
    {
        get => Data.Tags;
        init => _data = Data with { Tags = value };
    }

    public BookFormatModel Format
    {
        get => Data.Format;
        init => _data = Data with { Format = value };
    }

    public BookStorageInfoModel Storage
    {
        get => Data.Storage;
        init => _data = Data with { Storage = value ?? new BookStorageInfoModel() };
    }

    public string ManagedPath
    {
        get => Data.Storage.ManagedRelativePath;
        init => _data = Data with
        {
            Storage = Data.Storage with
            {
                ManagedRelativePath = value
            }
        };
    }

    public string? CoverPath
    {
        get => Data.Storage.CoverRelativePath;
        init => _data = Data with
        {
            Storage = Data.Storage with
            {
                CoverRelativePath = value
            }
        };
    }

    public string ManagedFileName => Data.Storage.ManagedFileName;
    public bool HasCoverResource => Data.Storage.HasCoverResource;

    public ulong SizeBytes
    {
        get => Data.SizeBytes;
        init => _data = Data with { SizeBytes = value };
    }

    public DateTimeOffset AddedAtUtc
    {
        get => Data.AddedAtUtc;
        init => _data = Data with { AddedAtUtc = value };
    }

    public IImage? ResolvedCoverImage
    {
        get => CoverPresentation.ResolvedCoverImage;
        set => CoverPresentation.ResolvedCoverImage = value;
    }

    public IBrush? CoverBackgroundBrush
    {
        get => CoverPresentation.CoverBackgroundBrush;
        set => CoverPresentation.CoverBackgroundBrush = value;
    }

    public string CoverPlaceholderText
    {
        get => CoverPresentation.CoverPlaceholderText;
        set => CoverPresentation.CoverPlaceholderText = value;
    }

    public bool IsSelected
    {
        get => CardPresentation.IsSelected;
        set => CardPresentation.IsSelected = value;
    }

    public IBrush? CardBackgroundBrush
    {
        get => CardPresentation.CardBackgroundBrush;
        set => CardPresentation.CardBackgroundBrush = value;
    }

    public IBrush? CardBorderBrush
    {
        get => CardPresentation.CardBorderBrush;
        set => CardPresentation.CardBorderBrush = value;
    }

    public Thickness CardBorderThickness
    {
        get => CardPresentation.CardBorderThickness;
        set => CardPresentation.CardBorderThickness = value;
    }

    public string AuthorsText => Authors.Count == 0 ? "Unknown author" : string.Join(", ", Authors);
    public string TagsText => Tags.Count == 0 ? "No tags" : string.Join(", ", Tags);
    public string FormatText => Format.ToString().ToUpperInvariant();
    public bool HasResolvedCover => CoverPresentation.HasResolvedCover;
    public bool ShowCoverPlaceholder => CoverPresentation.ShowCoverPlaceholder;

    public event PropertyChangedEventHandler? PropertyChanged;

    public BookListItemModel()
    {
        CoverPresentation.PropertyChanged += OnCoverPresentationPropertyChanged;
        CardPresentation.PropertyChanged += OnCardPresentationPropertyChanged;
    }

    private void OnCoverPresentationPropertyChanged(object? sender, PropertyChangedEventArgs eventArgs)
    {
        if (string.IsNullOrWhiteSpace(eventArgs.PropertyName))
        {
            return;
        }

        switch (eventArgs.PropertyName)
        {
        case nameof(BookCoverPresentationModel.ResolvedCoverImage):
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(ResolvedCoverImage)));
            break;
        case nameof(BookCoverPresentationModel.CoverBackgroundBrush):
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(CoverBackgroundBrush)));
            break;
        case nameof(BookCoverPresentationModel.CoverPlaceholderText):
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(CoverPlaceholderText)));
            break;
        case nameof(BookCoverPresentationModel.HasResolvedCover):
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(HasResolvedCover)));
            break;
        case nameof(BookCoverPresentationModel.ShowCoverPlaceholder):
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(ShowCoverPlaceholder)));
            break;
        }
    }

    private void OnCardPresentationPropertyChanged(object? sender, PropertyChangedEventArgs eventArgs)
    {
        if (string.IsNullOrWhiteSpace(eventArgs.PropertyName))
        {
            return;
        }

        switch (eventArgs.PropertyName)
        {
        case nameof(BookCardPresentationModel.IsSelected):
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsSelected)));
            break;
        case nameof(BookCardPresentationModel.CardBackgroundBrush):
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(CardBackgroundBrush)));
            break;
        case nameof(BookCardPresentationModel.CardBorderBrush):
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(CardBorderBrush)));
            break;
        case nameof(BookCardPresentationModel.CardBorderThickness):
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(CardBorderThickness)));
            break;
        }
    }
}

internal sealed record BookDetailsDataModel
{
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
    public BookStorageInfoModel Storage { get; init; } = new();
    public ulong SizeBytes { get; init; }
    public DateTimeOffset AddedAtUtc { get; init; }

    public string ManagedPath
    {
        get => Storage.ManagedRelativePath;
        init => Storage = Storage with { ManagedRelativePath = value };
    }

    public string? CoverPath
    {
        get => Storage.CoverRelativePath;
        init => Storage = Storage with { CoverRelativePath = value };
    }
}

internal sealed class BookDetailsModel : INotifyPropertyChanged
{
    private BookDetailsDataModel _data = new();

    public BookDetailsDataModel Data
    {
        get => _data;
        init => _data = value ?? new BookDetailsDataModel();
    }

    public BookCoverPresentationModel CoverPresentation { get; init; } = new();

    public long BookId
    {
        get => Data.BookId;
        init => _data = Data with { BookId = value };
    }

    public string Title
    {
        get => Data.Title;
        init => _data = Data with { Title = value };
    }

    public IReadOnlyList<string> Authors
    {
        get => Data.Authors;
        init => _data = Data with { Authors = value };
    }

    public string Language
    {
        get => Data.Language;
        init => _data = Data with { Language = value };
    }

    public string? Series
    {
        get => Data.Series;
        init => _data = Data with { Series = value };
    }

    public double? SeriesIndex
    {
        get => Data.SeriesIndex;
        init => _data = Data with { SeriesIndex = value };
    }

    public string? Publisher
    {
        get => Data.Publisher;
        init => _data = Data with { Publisher = value };
    }

    public int? Year
    {
        get => Data.Year;
        init => _data = Data with { Year = value };
    }

    public string? Isbn
    {
        get => Data.Isbn;
        init => _data = Data with { Isbn = value };
    }

    public IReadOnlyList<string> Tags
    {
        get => Data.Tags;
        init => _data = Data with { Tags = value };
    }

    public string? Description
    {
        get => Data.Description;
        init => _data = Data with { Description = value };
    }

    public string? Identifier
    {
        get => Data.Identifier;
        init => _data = Data with { Identifier = value };
    }

    public BookFormatModel Format
    {
        get => Data.Format;
        init => _data = Data with { Format = value };
    }

    public BookStorageInfoModel Storage
    {
        get => Data.Storage;
        init => _data = Data with { Storage = value ?? new BookStorageInfoModel() };
    }

    public string ManagedPath
    {
        get => Data.Storage.ManagedRelativePath;
        init => _data = Data with
        {
            Storage = Data.Storage with
            {
                ManagedRelativePath = value
            }
        };
    }

    public string? CoverPath
    {
        get => Data.Storage.CoverRelativePath;
        init => _data = Data with
        {
            Storage = Data.Storage with
            {
                CoverRelativePath = value
            }
        };
    }

    public ulong SizeBytes
    {
        get => Data.SizeBytes;
        init => _data = Data with { SizeBytes = value };
    }

    public string ManagedFileName => Data.Storage.ManagedFileName;
    public bool HasCoverResource => Data.Storage.HasCoverResource;
    public bool HasContentHash => Data.Storage.HasContentHash;

    public DateTimeOffset AddedAtUtc
    {
        get => Data.AddedAtUtc;
        init => _data = Data with { AddedAtUtc = value };
    }

    public IImage? ResolvedCoverImage
    {
        get => CoverPresentation.ResolvedCoverImage;
        set => CoverPresentation.ResolvedCoverImage = value;
    }

    public IBrush? CoverBackgroundBrush
    {
        get => CoverPresentation.CoverBackgroundBrush;
        set => CoverPresentation.CoverBackgroundBrush = value;
    }

    public string CoverPlaceholderText
    {
        get => CoverPresentation.CoverPlaceholderText;
        set => CoverPresentation.CoverPlaceholderText = value;
    }

    public bool HasResolvedCover => CoverPresentation.HasResolvedCover;
    public bool ShowCoverPlaceholder => CoverPresentation.ShowCoverPlaceholder;

    public event PropertyChangedEventHandler? PropertyChanged;

    public BookDetailsModel()
    {
        CoverPresentation.PropertyChanged += OnCoverPresentationPropertyChanged;
    }

    private void OnCoverPresentationPropertyChanged(object? sender, PropertyChangedEventArgs eventArgs)
    {
        if (string.IsNullOrWhiteSpace(eventArgs.PropertyName))
        {
            return;
        }

        switch (eventArgs.PropertyName)
        {
        case nameof(BookCoverPresentationModel.ResolvedCoverImage):
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(ResolvedCoverImage)));
            break;
        case nameof(BookCoverPresentationModel.CoverBackgroundBrush):
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(CoverBackgroundBrush)));
            break;
        case nameof(BookCoverPresentationModel.CoverPlaceholderText):
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(CoverPlaceholderText)));
            break;
        case nameof(BookCoverPresentationModel.HasResolvedCover):
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(HasResolvedCover)));
            break;
        case nameof(BookCoverPresentationModel.ShowCoverPlaceholder):
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(ShowCoverPlaceholder)));
            break;
        }
    }
}

internal sealed class LibraryStatisticsModel
{
    public ulong BookCount { get; init; }
    public ulong TotalLibrarySizeBytes { get; init; }
}

internal enum DeleteDestinationModel
{
    RecycleBin,
    ManagedTrash
}

internal sealed class DeleteBookResultModel
{
    public long BookId { get; init; }
    public DeleteDestinationModel Destination { get; init; }
    public bool HasOrphanedFiles { get; init; }
}
