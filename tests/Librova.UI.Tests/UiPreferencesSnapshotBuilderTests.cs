using Librova.UI.CoreHost;
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
            ConverterMode = UiConverterMode.BuiltInFb2Cng,
            Fb2CngExecutablePath = @"C:\Tools\fbc.exe",
            Fb2CngConfigPath = @"C:\Tools\fbc.yaml",
            ForceEpubConversionOnImport = true
        };

        var updated = UiPreferencesSnapshotBuilder.WithPreferredLibraryRoot(existing, @"D:\Librova\New");

        Assert.Equal(@"D:\Librova\New", updated.PreferredLibraryRoot);
        Assert.Equal(UiConverterMode.BuiltInFb2Cng, updated.ConverterMode);
        Assert.Equal(@"C:\Tools\fbc.exe", updated.Fb2CngExecutablePath);
        Assert.Equal(@"C:\Tools\fbc.yaml", updated.Fb2CngConfigPath);
        Assert.True(updated.ForceEpubConversionOnImport);
    }

    [Fact]
    public void WithPreferredLibraryRoot_DisablesLegacyConverterStateWithoutExecutablePath()
    {
        var existing = new UiPreferencesSnapshot
        {
            PreferredLibraryRoot = @"C:\Libraries\Old",
            ConverterMode = UiConverterMode.BuiltInFb2Cng,
            ForceEpubConversionOnImport = true
        };

        var updated = UiPreferencesSnapshotBuilder.WithPreferredLibraryRoot(existing, @"D:\Librova\New");

        Assert.Equal(@"D:\Librova\New", updated.PreferredLibraryRoot);
        Assert.Equal(UiConverterMode.Disabled, updated.ConverterMode);
        Assert.Null(updated.Fb2CngExecutablePath);
        Assert.Null(updated.Fb2CngConfigPath);
        Assert.False(updated.ForceEpubConversionOnImport);
    }
}
