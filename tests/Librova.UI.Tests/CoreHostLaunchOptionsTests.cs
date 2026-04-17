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
            HostLogFilePath = @"C:\Users\evgen\AppData\Local\Librova\RuntimeLogs\ABCDEF1234567890\host.log",
            ConverterWorkingDirectory = @"C:\Users\evgen\AppData\Local\Librova\Runtime\ABCDEF1234567890\ConverterWorkspace",
            ManagedStorageStagingRoot = @"C:\Users\evgen\AppData\Local\Librova\Runtime\ABCDEF1234567890\ManagedStorageStaging",
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
    public void Validate_RejectsRelativeConverterWorkingDirectory()
    {
        var options = new CoreHostLaunchOptions
        {
            ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
            PipePath = @"\\.\pipe\Librova.Test",
            LibraryRoot = @"C:\Libraries\Librova",
            ConverterWorkingDirectory = @"Runtime\ConverterWorkspace",
            ConverterMode = UiConverterMode.Disabled
        };

        var error = Assert.Throws<InvalidOperationException>(() => options.Validate());
        Assert.Contains("converter working directory", error.Message, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void Validate_RejectsRelativeManagedStorageStagingRoot()
    {
        var options = new CoreHostLaunchOptions
        {
            ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
            PipePath = @"\\.\pipe\Librova.Test",
            LibraryRoot = @"C:\Libraries\Librova",
            ManagedStorageStagingRoot = @"Runtime\ManagedStorageStaging",
            ConverterMode = UiConverterMode.Disabled
        };

        var error = Assert.Throws<InvalidOperationException>(() => options.Validate());
        Assert.Contains("managed storage staging root", error.Message, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void Validate_RejectsRelativeHostLogFilePath()
    {
        var options = new CoreHostLaunchOptions
        {
            ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
            PipePath = @"\\.\pipe\Librova.Test",
            LibraryRoot = @"C:\Libraries\Librova",
            HostLogFilePath = @"RuntimeLogs\host.log",
            ConverterMode = UiConverterMode.Disabled
        };

        var error = Assert.Throws<InvalidOperationException>(() => options.Validate());
        Assert.Contains("host log file path", error.Message, StringComparison.OrdinalIgnoreCase);
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
        Assert.False(string.IsNullOrWhiteSpace(options.HostLogFilePath));
        Assert.False(string.IsNullOrWhiteSpace(options.ConverterWorkingDirectory));
        Assert.False(string.IsNullOrWhiteSpace(options.ManagedStorageStagingRoot));
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
