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
}
