using Librova.UI.CoreHost;
using Xunit;

namespace Librova.UI.Tests;

public sealed class CoreHostProcessTests
{
    [Fact]
    public async Task StartAsync_StartsCoreHostAndWaitsForPipeReadiness()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-tests",
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var options = new CoreHostLaunchOptions
            {
                ExecutablePath = CoreHostPathResolver.ResolveDevelopmentExecutablePath(Path.Combine(
                    AppContext.BaseDirectory,
                    "..",
                    "..",
                    "..",
                    "..")),
                PipePath = $@"\\.\pipe\Librova.UI.Tests.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library")
            };

            await using var process = new CoreHostProcess();
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(15));

            await process.StartAsync(options, cancellation.Token);

            Assert.True(Directory.Exists(Path.Combine(options.LibraryRoot, "Logs")));
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, recursive: true);
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
