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

    [Fact]
    public async Task Reinitialize_MergesPreviousLogIntoNewFile()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-logging-tests",
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(sandboxRoot);

        var bootstrapLogPath = Path.Combine(sandboxRoot, "bootstrap-ui.log");
        var libraryLogPath = Path.Combine(sandboxRoot, "Library", "Logs", "ui.log");

        try
        {
            UiLogging.ReinitializeForTests(bootstrapLogPath);
            UiLogging.Information("Bootstrap message");
            UiLogging.Reinitialize(libraryLogPath);
            UiLogging.Information("Library message");
            UiLogging.Shutdown();

            Assert.False(File.Exists(bootstrapLogPath));
            Assert.True(File.Exists(libraryLogPath));

            var contents = await File.ReadAllTextAsync(libraryLogPath);
            Assert.Contains("Bootstrap message", contents);
            Assert.Contains("Library message", contents);
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
