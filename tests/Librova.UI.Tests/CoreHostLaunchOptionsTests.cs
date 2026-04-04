using Librova.UI.CoreHost;
using Librova.UI.Shell;
using Xunit;

namespace Librova.UI.Tests;

public sealed class CoreHostLaunchOptionsTests
{
    [Fact]
    public void Validate_AcceptsAbsoluteExecutablePipeAndLibraryRoot()
    {
        var options = new CoreHostLaunchOptions
        {
            ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
            PipePath = @"\\.\pipe\Librova.Test",
            LibraryRoot = @"C:\Libraries\Librova",
            LibraryOpenMode = UiLibraryOpenMode.OpenExisting,
            ConverterMode = UiConverterMode.Disabled
        };

        options.Validate();
    }

    [Fact]
    public void Validate_AcceptsCreateNewLibraryMode()
    {
        var options = new CoreHostLaunchOptions
        {
            ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
            PipePath = @"\\.\pipe\Librova.Test",
            LibraryRoot = @"C:\Libraries\Librova-New",
            LibraryOpenMode = UiLibraryOpenMode.CreateNew,
            ConverterMode = UiConverterMode.Disabled
        };

        options.Validate();
    }

    [Fact]
    public void Validate_RejectsRelativeExecutablePath()
    {
        var options = new CoreHostLaunchOptions
        {
            ExecutablePath = @"LibrovaCoreHostApp.exe",
            PipePath = @"\\.\pipe\Librova.Test",
            LibraryRoot = @"C:\Libraries\Librova",
            ConverterMode = UiConverterMode.Disabled
        };

        var error = Assert.Throws<InvalidOperationException>(() => options.Validate());
        Assert.Contains("absolute", error.Message, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void Validate_RejectsNonPipePath()
    {
        var options = new CoreHostLaunchOptions
        {
            ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
            PipePath = @"C:\Temp\pipe.txt",
            LibraryRoot = @"C:\Libraries\Librova",
            ConverterMode = UiConverterMode.Disabled
        };

        var error = Assert.Throws<InvalidOperationException>(() => options.Validate());
        Assert.Contains("named pipe", error.Message, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void Validate_RejectsBuiltInConverterWithRelativeExecutable()
    {
        var options = new CoreHostLaunchOptions
        {
            ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
            PipePath = @"\\.\pipe\Librova.Test",
            LibraryRoot = @"C:\Libraries\Librova",
            ConverterMode = UiConverterMode.BuiltInFb2Cng,
            Fb2CngExecutablePath = @"fbc.exe"
        };

        var error = Assert.Throws<InvalidOperationException>(() => options.Validate());
        Assert.Contains("converter executable", error.Message, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void Validate_RejectsUnknownConverterMode()
    {
        var options = new CoreHostLaunchOptions
        {
            ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
            PipePath = @"\\.\pipe\Librova.Test",
            LibraryRoot = @"C:\Libraries\Librova",
            ConverterMode = (UiConverterMode)999
        };

        var error = Assert.Throws<InvalidOperationException>(() => options.Validate());
        Assert.Contains("converter mode", error.Message, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void CreateDevelopmentDefaults_UsesProvidedPreferencesStoreForConverterSettings()
    {
        var options = CoreHostDevelopmentDefaults.Create(
            baseDirectory: AppContext.BaseDirectory,
            preferencesStore: new FakePreferencesStore
            {
                Snapshot = new UiPreferencesSnapshot
                {
                    PreferredLibraryRoot = @"D:\Librova\Data",
                    ConverterMode = UiConverterMode.BuiltInFb2Cng,
                    Fb2CngExecutablePath = @"D:\Tools\fbc.exe"
                }
            });

        Assert.Equal(@"D:\Librova\Data", options.LibraryRoot);
        Assert.StartsWith(@"Local\Librova.UI.Shutdown.", options.ShutdownEventName, StringComparison.Ordinal);
        Assert.Equal(UiConverterMode.BuiltInFb2Cng, options.ConverterMode);
        Assert.Equal(@"D:\Tools\fbc.exe", options.Fb2CngExecutablePath);
    }

    private sealed class FakePreferencesStore : IUiPreferencesStore
    {
        public UiPreferencesSnapshot? Snapshot { get; init; }

        public UiPreferencesSnapshot? TryLoad() => Snapshot;

        public void Save(UiPreferencesSnapshot snapshot)
        {
        }

        public void Clear()
        {
        }
    }
}
