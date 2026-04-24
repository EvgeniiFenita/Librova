using Avalonia.Controls;
using Avalonia.Input;
using Librova.UI.Views;
using System.Xml.Linq;
using Xunit;

namespace Librova.UI.Tests;

public sealed class MainWindowTests
{
    private static readonly XNamespace AvaloniaNamespace = "https://github.com/avaloniaui";

    [Fact]
    public void MainWindow_HandlesKeyDownAtWindowLevel_ForLibraryShortcuts()
    {
        var document = LoadMainWindowMarkup();

        Assert.Equal("OnWindowKeyDown", document.Root!.Attribute("KeyDown")?.Value);
    }

    [Fact]
    public void MainWindow_HasNoKeyBindingsForEscapeOrDelete()
    {
        var document = LoadMainWindowMarkup();
        var keyBindings = document
            .Descendants(AvaloniaNamespace + "KeyBinding")
            .ToArray();

        Assert.DoesNotContain(keyBindings, e => e.Attribute("Gesture")?.Value == "Escape");
        Assert.DoesNotContain(keyBindings, e => e.Attribute("Gesture")?.Value == "Delete");
    }

    [Fact]
    public void MainWindow_CollectionDialogs_AreHiddenUntilShellIsRunning()
    {
        var document = LoadMainWindowMarkup();
        var createDialog = FindElementByIsVisibleBinding(
            document,
            "{Binding Shell.LibraryBrowser.IsCreateCollectionDialogOpen}");
        var deleteDialog = FindElementByIsVisibleBinding(
            document,
            "{Binding Shell.LibraryBrowser.IsDeleteCollectionDialogOpen}");

        Assert.Contains(createDialog.Ancestors(), HasShellVisibilityBinding);
        Assert.Contains(deleteDialog.Ancestors(), HasShellVisibilityBinding);
    }

    [Fact]
    public void MainWindow_CollectionsSidebar_UsesContextMenuForDeleteAction()
    {
        var document = LoadMainWindowMarkup();
        // Delete is now via right-click context menu on each collection item (code-behind),
        // not a static button in the sidebar header — verify no inline delete button exists.
        var inlineDeleteButtons = document
            .Descendants(AvaloniaNamespace + "Button")
            .Where(element => element.Attribute("Command")?.Value == "{Binding Shell.LibraryBrowser.RequestDeleteCollectionCommand}")
            .ToArray();
        var backToLibraryButtons = document
            .Descendants(AvaloniaNamespace + "Button")
            .Where(element => element.Attribute("Content")?.Value == "Back to Library")
            .ToArray();

        Assert.Empty(inlineDeleteButtons);
        Assert.Empty(backToLibraryButtons);
    }

    [Fact]
    public void MainWindow_PrimaryNavButtons_UseIndependentNavigationState()
    {
        var document = LoadMainWindowMarkup();
        var libraryButton = document
            .Descendants(AvaloniaNamespace + "Button")
            .Single(element => element.Attribute("Command")?.Value == "{Binding Shell.ShowLibrarySectionCommand}");

        var importButton = document
            .Descendants(AvaloniaNamespace + "Button")
            .Single(element => element.Attribute("Command")?.Value == "{Binding Shell.ShowImportSectionCommand}");
        var settingsButton = document
            .Descendants(AvaloniaNamespace + "Button")
            .Single(element => element.Attribute("Command")?.Value == "{Binding Shell.ShowSettingsSectionCommand}");

        Assert.Equal("{Binding Shell.IsLibraryNavigationActive}", libraryButton.Attribute("Classes.Active")?.Value);
        Assert.Equal("{Binding Shell.IsImportNavigationActive}", importButton.Attribute("Classes.Active")?.Value);
        Assert.Equal("{Binding Shell.IsSettingsNavigationActive}", settingsButton.Attribute("Classes.Active")?.Value);
    }

    [Fact]
    public void MainWindow_CollectionNavButtons_MatchPrimaryNavButtonStructure()
    {
        var document = LoadMainWindowMarkup();
        var collectionButton = document
            .Descendants(AvaloniaNamespace + "Button")
            .Single(element => element.Attribute("Command")?.Value == "{Binding #MainWindowRoot.DataContext.Shell.LibraryBrowser.SelectCollectionCommand}");
        var grid = Assert.Single(collectionButton.Elements(AvaloniaNamespace + "Grid"));

        // NavIcon is wrapped in a Border.NavIconBadge for the active amber-tint effect
        var iconBadge = Assert.Single(grid.Elements(AvaloniaNamespace + "Border"),
            element => element.Attribute("Classes")?.Value == "NavIconBadge");
        var icon = Assert.Single(iconBadge.Elements(AvaloniaNamespace + "TextBlock"),
            element => element.Attribute("Classes")?.Value == "NavIcon");
        var label = Assert.Single(grid.Elements(AvaloniaNamespace + "TextBlock"),
            element => element.Attribute("Classes")?.Value == "NavLabel");

        Assert.Equal("NavItem", collectionButton.Attribute("Classes")?.Value);
        Assert.Equal("{Binding IsSelected}", collectionButton.Attribute("Classes.Active")?.Value);
        Assert.Equal("{Binding Name}", collectionButton.Attribute("ToolTip.Tip")?.Value);
        Assert.Equal("Auto,*", grid.Attribute("ColumnDefinitions")?.Value);
        Assert.Equal("10", grid.Attribute("ColumnSpacing")?.Value);
        Assert.Equal("{Binding IconGlyph}", icon.Attribute("Text")?.Value);
        Assert.Equal("1", label.Attribute("Grid.Column")?.Value);
        Assert.Equal("132", label.Attribute("MaxWidth")?.Value);
        Assert.Equal("NavLabel", label.Attribute("Classes")?.Value);
        Assert.Equal("{Binding Name}", label.Attribute("Text")?.Value);
        Assert.Equal("CharacterEllipsis", label.Attribute("TextTrimming")?.Value);
        Assert.Equal("NoWrap", label.Attribute("TextWrapping")?.Value);
    }

    [Fact]
    public void MainWindow_TextBoxShortcutSource_IsIgnoredForDeleteShortcut()
    {
        Assert.True(MainWindow.IsTextInputShortcutSource(new TextBox()));
    }

    [Fact]
    public void MainWindow_NonTextBoxShortcutSource_CanUseDeleteShortcut()
    {
        Assert.False(MainWindow.IsTextInputShortcutSource(new Button()));
    }

    [Fact]
    public void NormalizeDroppedSourcePaths_FiltersEmptyEntries_AndDeduplicatesCaseInsensitively()
    {
        var result = MainWindow.NormalizeDroppedSourcePaths(
            [
                null,
                "",
                "   ",
                @"C:\Books\One.fb2",
                @"C:\Books\one.fb2",
                @"C:\Books\Two.epub"
            ]);

        Assert.Equal(
            [@"C:\Books\One.fb2", @"C:\Books\Two.epub"],
            result);
    }

    [Fact]
    public void GetDropEffect_ReturnsNone_ForEmptyResolvedSourceList()
    {
        var effect = MainWindow.GetDropEffect([]);

        Assert.Equal(DragDropEffects.None, effect);
    }

    [Fact]
    public void GetDropEffect_ReturnsCopy_ForAnyNonEmptySourceList()
    {
        var effect = MainWindow.GetDropEffect([@"C:\Books\One.fb2"]);

        Assert.Equal(DragDropEffects.Copy, effect);
    }

    private static XDocument LoadMainWindowMarkup()
    {
        var repositoryRoot = ResolveRepositoryRoot();
        var markupPath = Path.Combine(repositoryRoot.FullName, "apps", "Librova.UI", "Views", "MainWindow.axaml");
        return XDocument.Load(markupPath);
    }

    private static DirectoryInfo ResolveRepositoryRoot()
    {
        var directory = new DirectoryInfo(AppContext.BaseDirectory);
        while (directory is not null)
        {
            var candidate = Path.Combine(directory.FullName, "apps", "Librova.UI", "Views", "MainWindow.axaml");
            if (File.Exists(candidate))
            {
                return directory;
            }

            directory = directory.Parent;
        }

        throw new DirectoryNotFoundException("Could not resolve repository root from the test output directory.");
    }

    private static XElement FindElementByIsVisibleBinding(XDocument document, string binding) =>
        document
            .Descendants()
            .Single(element => element.Attribute("IsVisible")?.Value == binding);

    private static bool HasShellVisibilityBinding(XElement element) =>
        element.Name == AvaloniaNamespace + "Grid"
        && element.Attribute("IsVisible")?.Value == "{Binding HasShell}";
}
