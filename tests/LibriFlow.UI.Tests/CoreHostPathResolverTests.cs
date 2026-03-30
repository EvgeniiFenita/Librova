using LibriFlow.UI.CoreHost;
using Xunit;

namespace LibriFlow.UI.Tests;

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
        Assert.Equal("LibriFlowCoreHostApp.exe", Path.GetFileName(path));
    }
}
