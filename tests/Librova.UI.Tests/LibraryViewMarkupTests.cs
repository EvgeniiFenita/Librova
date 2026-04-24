using Avalonia.Controls;
using Avalonia.Threading;
using Librova.UI.LibraryCatalog;
using Librova.UI.ViewModels;
using Librova.UI.Views;
using System.Collections;
using System.Xml.Linq;
using Xunit;

namespace Librova.UI.Tests;

public sealed class LibraryViewMarkupTests
{
    private static readonly XNamespace AvaloniaNamespace = "https://github.com/avaloniaui";
    private static readonly XNamespace XNamespace = "http://schemas.microsoft.com/winfx/2006/xaml";

    [Fact]
    public void LibraryView_EmptyState_GoToImportButton_IsBoundToEmptyLibraryState()
    {
        var document = LoadLibraryViewMarkup();
        var button = FindElementByName(document, "GoToImportButton");

        Assert.Equal("Button", button.Name.LocalName);
        Assert.Equal("{Binding LibraryBrowser.ShowGoToImportButton}", button.Attribute("IsVisible")?.Value);
        Assert.Equal("{Binding LibraryBrowser.GoToImportCommand}", button.Attribute("Command")?.Value);
    }

    [Fact]
    public void LibraryView_EmptyState_NoActionButton_IsBoundToNoResultsState()
    {
        var document = LoadLibraryViewMarkup();
        var buttonsBoundToNoResultsState = document
            .Descendants(AvaloniaNamespace + "Button")
            .Where(element => element.Attribute("IsVisible")?.Value == "{Binding LibraryBrowser.ShowClearFiltersButton}")
            .ToArray();

        Assert.Empty(buttonsBoundToNoResultsState);
    }

    [Fact]
    public void LibraryView_EmptyState_Illustrations_AreBoundToDistinctStates()
    {
        var document = LoadLibraryViewMarkup();
        var emptyLibraryIllustration = FindElementByName(document, "EmptyLibraryIllustration");
        var noResultsIllustration = FindElementByName(document, "NoResultsIllustration");

        Assert.Equal("{Binding LibraryBrowser.ShowGoToImportButton}", emptyLibraryIllustration.Attribute("IsVisible")?.Value);
        Assert.Equal("{Binding LibraryBrowser.ShowClearFiltersButton}", noResultsIllustration.Attribute("IsVisible")?.Value);
    }

    [Fact]
    public void LibraryView_BookCard_ContextMenu_UsesExpectedCommandsAndItems()
    {
        var document = LoadLibraryViewMarkup();
        var bookCardButton = document
            .Descendants(AvaloniaNamespace + "Button")
            .Single(element => element.Attribute("Classes")?.Value == "BookCardButton");

        Assert.Equal("OnBookCardContextRequested", bookCardButton.Attribute("ContextRequested")?.Value);
        Assert.Empty(bookCardButton.Descendants(AvaloniaNamespace + "ContextMenu"));
    }

    [Fact]
    public void LibraryView_BookCard_ContextMenu_BuildsProgrammaticCollectionMenu()
    {
        var favorites = new BookCollectionModel(10, "Favorites", "star", BookCollectionKindModel.User, true);
        var unread = new BookCollectionModel(11, "Unread", "bookmark", BookCollectionKindModel.User, true);
        var longNamedCollection = new BookCollectionModel(
            12,
            "Very Long Collection Name That Should Be Trimmed In Context Menus",
            "folder",
            BookCollectionKindModel.User,
            true);
        var browser = new LibraryBrowserViewModel();
        browser.Collections.Add(new BookCollectionListItemViewModel(favorites));
        browser.Collections.Add(new BookCollectionListItemViewModel(unread));
        browser.Collections.Add(new BookCollectionListItemViewModel(longNamedCollection));

        var book = new BookListItemModel
        {
            BookId = 7,
            Title = "Alpha",
            Authors = ["Author"],
            Language = "en",
            Format = BookFormatModel.Epub,
            ManagedPath = "Objects/aa/aa/0000000007.book.epub",
            SizeBytes = 1024,
            AddedAtUtc = DateTimeOffset.UtcNow,
            Collections = [favorites]
        };

        var menu = Dispatcher.UIThread.Invoke(() => LibraryView.BuildBookCardContextMenu(browser, book));
        var menuItems = GetItems(menu.ItemsSource);
        var commandItems = menuItems.OfType<MenuItem>().ToArray();
        var addToItem = Assert.Single(commandItems, item => item.Header?.ToString() == "Add to");
        var addToItems = addToItem.Items.Cast<object>().ToArray();
        var collectionItems = addToItems.OfType<MenuItem>().ToArray();

        Assert.Contains("BookCardContextMenu", menu.Classes);
        Assert.Contains(commandItems, item => item.Header?.ToString() == "Export");
        Assert.Contains(commandItems, item => item.Header?.ToString() == "Copy Title");
        Assert.Contains(commandItems, item => item.Header?.ToString() == "Move to Trash");
        Assert.DoesNotContain(commandItems, item => item.Header?.ToString() == "Remove from collection");
        Assert.Contains(addToItems, item => item is Separator);
        Assert.NotNull(addToItem.Icon);

        var existingMembershipItem = Assert.Single(collectionItems, item => item.Header?.ToString() == "✓ Favorites");
        var newMembershipItem = Assert.Single(collectionItems, item => item.Header?.ToString() == "Unread");
        var longMembershipItem = Assert.Single(
            collectionItems,
            item => item.CommandParameter is BookCollectionMembershipRequest request
                && request.Collection.CollectionId == longNamedCollection.CollectionId);
        var createNewItem = Assert.Single(collectionItems, item => item.Header?.ToString() == "Create new...");

        Assert.False(existingMembershipItem.IsEnabled);
        Assert.True(newMembershipItem.IsEnabled);
        Assert.EndsWith("...", longMembershipItem.Header?.ToString());
        Assert.True(longMembershipItem.Header?.ToString()?.Length <= 36);
        Assert.Equal(longNamedCollection.Name, ToolTip.GetTip(longMembershipItem));
        Assert.IsType<BookCollectionMembershipRequest>(existingMembershipItem.CommandParameter);
        Assert.IsType<BookCollectionMembershipRequest>(newMembershipItem.CommandParameter);
        Assert.IsType<BookCollectionMembershipRequest>(longMembershipItem.CommandParameter);
        Assert.Null(createNewItem.Command);
    }

    private static XElement FindElementByName(XDocument document, string name) =>
        document
            .Descendants()
            .Single(element => element.Attribute(XNamespace + "Name")?.Value == name);

    private static XDocument LoadLibraryViewMarkup()
    {
        var repositoryRoot = ResolveRepositoryRoot();
        var markupPath = Path.Combine(repositoryRoot.FullName, "apps", "Librova.UI", "Views", "LibraryView.axaml");
        return XDocument.Load(markupPath);
    }

    private static object[] GetItems(object? itemsSource)
    {
        var enumerable = Assert.IsAssignableFrom<IEnumerable>(itemsSource);
        return enumerable.Cast<object>().ToArray();
    }

    private static DirectoryInfo ResolveRepositoryRoot()
    {
        var directory = new DirectoryInfo(AppContext.BaseDirectory);
        while (directory is not null)
        {
            var candidate = Path.Combine(directory.FullName, "apps", "Librova.UI", "Views", "LibraryView.axaml");
            if (File.Exists(candidate))
            {
                return directory;
            }

            directory = directory.Parent;
        }

        throw new DirectoryNotFoundException("Could not resolve repository root from the test output directory.");
    }
}
