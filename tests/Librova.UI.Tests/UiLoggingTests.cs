using Librova.UI.Logging;
using Librova.UI.Shell;
using System.Linq;
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

    [Fact]
    public async Task Reinitialize_StreamsLargeBootstrapLogIntoLibraryLog()
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
            var lines = Enumerable.Range(1, 5000).Select(i => $"Bootstrap line {i}").ToArray();
            await File.WriteAllLinesAsync(bootstrapLogPath, lines);

            UiLogging.ReinitializeForTests(bootstrapLogPath);
            UiLogging.Reinitialize(libraryLogPath);
            UiLogging.Information("Post-merge library message");
            UiLogging.Shutdown();

            Assert.False(File.Exists(bootstrapLogPath));
            Assert.True(File.Exists(libraryLogPath));

            var contents = await File.ReadAllTextAsync(libraryLogPath);
            Assert.Contains("Bootstrap line 1", contents);
            Assert.Contains("Bootstrap line 2500", contents);
            Assert.Contains("Bootstrap line 5000", contents);
            Assert.Contains("Post-merge library message", contents);
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
    public void BootstrapLogging_DoesNotPolluteEmptyCreateLibraryTarget()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-logging-tests",
            Guid.NewGuid().ToString("N"));
        var bootstrapLogPath = Path.Combine(sandboxRoot, "bootstrap-ui.log");
        var libraryRoot = Path.Combine(sandboxRoot, "Library");
        Directory.CreateDirectory(libraryRoot);

        try
        {
            UiLogging.ReinitializeForTests(bootstrapLogPath);
            UiLogging.Information("Bootstrap validation message");
            UiLogging.Shutdown();

            Assert.Equal(string.Empty, LibraryRootInspection.BuildCreateNewValidationMessage(libraryRoot));
            Assert.Empty(Directory.EnumerateFileSystemEntries(libraryRoot));
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
