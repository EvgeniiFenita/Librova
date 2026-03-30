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

    [Fact]
    public void ResolveDevelopmentExecutablePath_UsesEnvironmentOverrideWhenProvided()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-path-resolver", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);
        var overridePath = Path.Combine(sandboxRoot, "LibrovaCoreHostApp.exe");
        File.WriteAllText(overridePath, string.Empty);

        try
        {
            var resolvedPath = CoreHostPathResolver.ResolveDevelopmentExecutablePath(Path.Combine(
                AppContext.BaseDirectory,
                "..",
                ".."),
                overridePath);

            Assert.Equal(Path.GetFullPath(overridePath), resolvedPath);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }
}
