using Librova.UI.CoreHost;
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
}
