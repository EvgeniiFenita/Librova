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
            CustomConverterExecutablePath = @"C:\Tools\custom.exe",
            CustomConverterArguments = ["--source", "{source}"],
            CustomConverterOutputMode = UiConverterOutputMode.SingleFileInDestinationDirectory,
            ForceEpubConversionOnImport = true
        };

        var updated = UiPreferencesSnapshotBuilder.WithPreferredLibraryRoot(existing, @"D:\Librova\New");

        Assert.Equal(@"D:\Librova\New", updated.PreferredLibraryRoot);
        Assert.Equal(UiConverterMode.BuiltInFb2Cng, updated.ConverterMode);
        Assert.Equal(@"C:\Tools\fbc.exe", updated.Fb2CngExecutablePath);
        Assert.Equal(@"C:\Tools\fbc.yaml", updated.Fb2CngConfigPath);
        Assert.Equal(@"C:\Tools\custom.exe", updated.CustomConverterExecutablePath);
        Assert.NotNull(updated.CustomConverterArguments);
        Assert.Equal(["--source", "{source}"], updated.CustomConverterArguments);
        Assert.Equal(UiConverterOutputMode.SingleFileInDestinationDirectory, updated.CustomConverterOutputMode);
        Assert.True(updated.ForceEpubConversionOnImport);
    }
}
