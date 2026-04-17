using Librova.UI.CoreHost;
using Librova.UI.LibraryCatalog;
using Librova.UI.Shell;
using Xunit;

namespace Librova.UI.Tests;

public sealed class UiPreferencesSnapshotBuilderTests
{
    [Fact]
    public void WithPreferredLibraryRoot_OverridesRootAndPreservesOtherFields()
    {
        var existing = new UiPreferencesSnapshot
        {
            PreferredLibraryRoot = @"C:\Libraries\Old",
            PortablePreferredLibraryRoot = @"..\Library",
            ConverterMode = UiConverterMode.BuiltInFb2Cng,
            Fb2CngExecutablePath = @"C:\Tools\fbc.exe",
            Fb2CngConfigPath = @"C:\Tools\fbc.yaml",
            ForceEpubConversionOnImport = true,
            PreferredSortDescending = true
        };

        var updated = UiPreferencesSnapshotBuilder.WithPreferredLibraryRoot(existing, @"D:\Librova\New");

        Assert.Equal(@"D:\Librova\New", updated.PreferredLibraryRoot);
        Assert.Null(updated.PortablePreferredLibraryRoot);
        Assert.Equal(UiConverterMode.BuiltInFb2Cng, updated.ConverterMode);
        Assert.Equal(@"C:\Tools\fbc.exe", updated.Fb2CngExecutablePath);
        Assert.Equal(@"C:\Tools\fbc.yaml", updated.Fb2CngConfigPath);
        Assert.True(updated.ForceEpubConversionOnImport);
        Assert.True(updated.PreferredSortDescending);
    }

    [Fact]
    public void WithPreferredLibraryRoot_DisablesLegacyConverterStateWithoutExecutablePath()
    {
        var existing = new UiPreferencesSnapshot
        {
            PreferredLibraryRoot = @"C:\Libraries\Old",
            PortablePreferredLibraryRoot = @"..\Library",
            ConverterMode = UiConverterMode.BuiltInFb2Cng,
            ForceEpubConversionOnImport = true
        };

        var updated = UiPreferencesSnapshotBuilder.WithPreferredLibraryRoot(existing, @"D:\Librova\New");

        Assert.Equal(@"D:\Librova\New", updated.PreferredLibraryRoot);
        Assert.Null(updated.PortablePreferredLibraryRoot);
        Assert.Equal(UiConverterMode.Disabled, updated.ConverterMode);
        Assert.Null(updated.Fb2CngExecutablePath);
        Assert.Null(updated.Fb2CngConfigPath);
        Assert.False(updated.ForceEpubConversionOnImport);
    }

    [Fact]
    public void WithPreferredLibraryRoot_PreservesPreferredSortKey()
    {
        var existing = new UiPreferencesSnapshot
        {
            PreferredLibraryRoot = @"C:\Libraries\Old",
            Fb2CngExecutablePath = @"C:\Tools\fbc.exe",
            PreferredSortKey = BookSortModel.DateAdded,
            PreferredSortDescending = true
        };

        var updated = UiPreferencesSnapshotBuilder.WithPreferredLibraryRoot(existing, @"D:\Librova\New");

        Assert.Equal(BookSortModel.DateAdded, updated.PreferredSortKey);
        Assert.True(updated.PreferredSortDescending);
    }
}

