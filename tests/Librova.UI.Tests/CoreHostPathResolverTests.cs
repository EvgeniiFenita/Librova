using Librova.UI.CoreHost;
using Xunit;

namespace Librova.UI.Tests;

public sealed class CoreHostPathResolverTests
{
    [Fact]
    public void ResolveDevelopmentExecutablePath_FindsHostRelativeToRepositoryLayout()
    {
        var baseDirectory = Path.Combine(
            AppContext.BaseDirectory,
            "..",
            "..",
            "..",
            "..");

        var path = CoreHostPathResolver.ResolveDevelopmentExecutablePath(baseDirectory);

        Assert.True(Path.IsPathFullyQualified(path));
        Assert.True(File.Exists(path));
        Assert.Equal("LibrovaCoreHostApp.exe", Path.GetFileName(path));
    }
}
