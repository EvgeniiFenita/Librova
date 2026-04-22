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
