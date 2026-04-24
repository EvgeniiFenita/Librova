using Librova.UI.LibraryCatalog;
using Librova.UI.Mvvm;
using System.Collections.Generic;
using System.Linq;

namespace Librova.UI.ViewModels;

internal sealed record BookCollectionIconOption(string Key, string Glyph, string Label);

internal static class BookCollectionIcons
{
    public static IReadOnlyList<BookCollectionIconOption> All { get; } =
    [
        new("shelf",    "📚", "Shelf"),
        new("stack",    "🗂", "Stack"),
        new("archive",  "📦", "Archive"),
        new("folder",   "📁", "Folder"),
        new("bookmark", "🔖", "Bookmark"),
        new("star",     "⭐", "Star"),
        new("heart",    "❤️", "Heart"),
        new("compass",  "🧭", "Compass"),
        new("map",      "🗺", "Map"),
        new("clock",    "⏰", "Clock"),
        new("spark",    "✨", "Spark"),
        new("moon",     "🌙", "Moon"),
        new("sun",      "☀️", "Sun"),
        new("leaf",     "🍃", "Leaf"),
        new("flame",    "🔥", "Flame"),
        new("crown",    "👑", "Crown"),
        new("quill",    "✏️", "Quill"),
        new("mask",     "🎭", "Mask"),
        new("tower",    "🏰", "Tower"),
        new("ring",     "💍", "Ring"),
    ];

    public static string ResolveGlyph(string? key) =>
        All.FirstOrDefault(option => option.Key == key)?.Glyph ?? All[0].Glyph;
}

internal sealed class BookCollectionListItemViewModel : ObservableObject
{
    private bool _isSelected;

    public BookCollectionListItemViewModel(BookCollectionModel collection)
    {
        Collection = collection;
    }

    public BookCollectionModel Collection { get; }

    public long CollectionId => Collection.CollectionId;
    public string Name => Collection.Name;
    public string IconKey => Collection.IconKey;
    public string IconGlyph => BookCollectionIcons.ResolveGlyph(Collection.IconKey);
    public bool IsDeletable => Collection.IsDeletable;

    public bool IsSelected
    {
        get => _isSelected;
        set => SetProperty(ref _isSelected, value);
    }
}

internal sealed record BookCollectionMembershipRequest(
    BookListItemModel Book,
    BookCollectionModel Collection,
    bool IsMember);
