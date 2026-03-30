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
            LibraryRoot = @"C:\Libraries\Librova"
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
            LibraryRoot = @"C:\Libraries\Librova"
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
            LibraryRoot = @"C:\Libraries\Librova"
        };

        var error = Assert.Throws<InvalidOperationException>(() => options.Validate());
        Assert.Contains("named pipe", error.Message, StringComparison.OrdinalIgnoreCase);
    }
}
