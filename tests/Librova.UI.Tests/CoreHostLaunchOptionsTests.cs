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
    public void Validate_AcceptsCustomConverterConfiguration()
    {
        var options = new CoreHostLaunchOptions
        {
            ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
            PipePath = @"\\.\pipe\Librova.Test",
            LibraryRoot = @"C:\Libraries\Librova",
            ConverterMode = UiConverterMode.CustomCommand,
            CustomConverterExecutablePath = @"C:\Tools\custom.exe",
            CustomConverterArguments = ["--input", "{source}", "--output-dir", "{destination_dir}"],
            CustomConverterOutputMode = UiConverterOutputMode.SingleFileInDestinationDirectory
        };

        options.Validate();
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
                    ConverterMode = UiConverterMode.CustomCommand,
                    CustomConverterExecutablePath = @"D:\Tools\custom.exe",
                    CustomConverterArguments = ["--input", "{source}"],
                    CustomConverterOutputMode = UiConverterOutputMode.SingleFileInDestinationDirectory
                }
            });

        Assert.Equal(@"D:\Librova\Data", options.LibraryRoot);
        Assert.Equal(UiConverterMode.CustomCommand, options.ConverterMode);
        Assert.Equal(@"D:\Tools\custom.exe", options.CustomConverterExecutablePath);
        Assert.Equal(["--input", "{source}"], options.CustomConverterArguments);
        Assert.Equal(UiConverterOutputMode.SingleFileInDestinationDirectory, options.CustomConverterOutputMode);
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
