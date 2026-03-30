using Librova.UI.Shell;
using Xunit;

namespace Librova.UI.Tests;

public sealed class ShellLaunchOptionsTests
{
    [Fact]
    public void FromArgs_UsesFirstNonEmptyArgumentAsInitialSourcePath()
    {
        var options = ShellLaunchOptions.FromArgs(["", "  ", @"C:\Incoming\book.fb2", @"C:\Ignored.epub"]);

        Assert.Equal(@"C:\Incoming\book.fb2", options.InitialSourcePath);
    }

    [Fact]
    public void FromArgs_LeavesInitialSourcePathEmptyWhenNoUsableArgumentsExist()
    {
        var options = ShellLaunchOptions.FromArgs(["", "   "]);

        Assert.Null(options.InitialSourcePath);
    }
}
