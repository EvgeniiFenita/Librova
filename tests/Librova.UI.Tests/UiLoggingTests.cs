using Librova.UI.Logging;
using Xunit;

namespace Librova.UI.Tests;

public sealed class UiLoggingTests
{
    [Fact]
    public async Task ReinitializeForTests_WritesLogEntriesIntoConfiguredFile()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-logging-tests",
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(sandboxRoot);

        var logFilePath = Path.Combine(sandboxRoot, "ui.log");

        try
        {
            UiLogging.ReinitializeForTests(logFilePath);
            UiLogging.Information("UI logging smoke message {Value}", 42);
            UiLogging.Shutdown();

            Assert.True(File.Exists(logFilePath));

            var contents = await File.ReadAllTextAsync(logFilePath);
            Assert.Contains("UI logging smoke message 42", contents);
        }
        finally
        {
            UiLogging.Shutdown();

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
