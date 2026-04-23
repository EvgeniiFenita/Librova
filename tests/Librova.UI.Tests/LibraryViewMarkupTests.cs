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
        var contextMenu = bookCardButton.Descendants(AvaloniaNamespace + "ContextMenu").Single();
        var menuItems = contextMenu.Descendants(AvaloniaNamespace + "MenuItem").ToArray();

        Assert.Equal("OnBookCardContextRequested", bookCardButton.Attribute("ContextRequested")?.Value);
        Assert.Equal("BookCardContextMenu", contextMenu.Attribute("Classes")?.Value);
        Assert.Equal(4, menuItems.Length);
        Assert.Equal("Export", menuItems[0].Attribute("Header")?.Value);
        Assert.Equal("{Binding #LibraryRootView.DataContext.LibraryBrowser.ExportBookFromCardCommand}", menuItems[0].Attribute("Command")?.Value);
        Assert.Equal("{Binding .}", menuItems[0].Attribute("CommandParameter")?.Value);
        Assert.Equal("Export as EPUB", menuItems[1].Attribute("Header")?.Value);
        Assert.Equal("{Binding #LibraryRootView.DataContext.HasConfiguredConverter}", menuItems[1].Attribute("IsVisible")?.Value);
        Assert.Equal("{Binding #LibraryRootView.DataContext.LibraryBrowser.ExportBookAsEpubFromCardCommand}", menuItems[1].Attribute("Command")?.Value);
        Assert.Equal("{Binding .}", menuItems[1].Attribute("CommandParameter")?.Value);
        Assert.Equal("Copy Title", menuItems[2].Attribute("Header")?.Value);
        Assert.Equal("{Binding #LibraryRootView.DataContext.LibraryBrowser.CopyBookTitleCommand}", menuItems[2].Attribute("Command")?.Value);
        Assert.Equal("{Binding .}", menuItems[2].Attribute("CommandParameter")?.Value);
        Assert.Equal("Move to Trash", menuItems[3].Attribute("Header")?.Value);
        Assert.Equal("{Binding #LibraryRootView.DataContext.LibraryBrowser.MoveBookToTrashFromCardCommand}", menuItems[3].Attribute("Command")?.Value);
        Assert.Equal("{Binding .}", menuItems[3].Attribute("CommandParameter")?.Value);
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
