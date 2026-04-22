using Librova.UI.Runtime;
using Librova.UI.Logging;
using Xunit;

namespace Librova.UI.Tests;

[Collection(UiLoggingCollection.Name)]
public sealed class RuntimeLogSynchronizationTests
{
    [Fact]
    public async Task SyncPendingRuntimeLogs_CopiesRuntimeLogsIntoLibraryAndDeletesRuntimeFiles()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-tests",
            $"{Guid.NewGuid():N}");
        var libraryRoot = Path.Combine(sandboxRoot, "Library");
        Directory.CreateDirectory(Path.Combine(libraryRoot, "Logs"));

        try
        {
            var uiRuntimeLogPath = RuntimeEnvironment.GetUiRuntimeLogFilePathForLibrary(libraryRoot, sandboxRoot, @"C:\Override\LibrovaCoreHostApp.exe");
            var hostRuntimeLogPath = RuntimeEnvironment.GetHostRuntimeLogFilePathForLibrary(libraryRoot, sandboxRoot, @"C:\Override\LibrovaCoreHostApp.exe");
            Directory.CreateDirectory(Path.GetDirectoryName(uiRuntimeLogPath)!);
            await File.WriteAllTextAsync(uiRuntimeLogPath, "ui runtime log");
            await File.WriteAllTextAsync(hostRuntimeLogPath, "host runtime log");

            RuntimeLogSynchronization.SyncPendingRuntimeLogs(libraryRoot);

            var uiRetainedLogPath = RuntimeEnvironment.GetUiLogFilePathForLibrary(libraryRoot);
            var hostRetainedLogPath = RuntimeEnvironment.GetHostLogFilePathForLibrary(libraryRoot);
            Assert.False(File.Exists(uiRuntimeLogPath));
            Assert.False(File.Exists(hostRuntimeLogPath));
            Assert.Equal("ui runtime log", await File.ReadAllTextAsync(uiRetainedLogPath));
            Assert.Equal("host runtime log", await File.ReadAllTextAsync(hostRetainedLogPath));
        }
        finally
        {
            TryDeleteDirectory(sandboxRoot);
        }
    }

    [Fact]
    public async Task SyncPendingRuntimeLogs_AppendsRuntimeLogsToExistingRetainedLogs()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-tests",
            $"{Guid.NewGuid():N}");
        var libraryRoot = Path.Combine(sandboxRoot, "Library");
        Directory.CreateDirectory(Path.Combine(libraryRoot, "Logs"));

        try
        {
            var uiRetainedLogPath = RuntimeEnvironment.GetUiLogFilePathForLibrary(libraryRoot);
            var hostRetainedLogPath = RuntimeEnvironment.GetHostLogFilePathForLibrary(libraryRoot);
            await File.WriteAllTextAsync(uiRetainedLogPath, "old ui log" + Environment.NewLine);
            await File.WriteAllTextAsync(hostRetainedLogPath, "old host log" + Environment.NewLine);

            var uiRuntimeLogPath = RuntimeEnvironment.GetUiRuntimeLogFilePathForLibrary(libraryRoot);
            var hostRuntimeLogPath = RuntimeEnvironment.GetHostRuntimeLogFilePathForLibrary(libraryRoot);
            Directory.CreateDirectory(Path.GetDirectoryName(uiRuntimeLogPath)!);
            await File.WriteAllTextAsync(uiRuntimeLogPath, "new ui log");
            await File.WriteAllTextAsync(hostRuntimeLogPath, "new host log");

            RuntimeLogSynchronization.SyncPendingRuntimeLogs(libraryRoot);

            var uiText = await File.ReadAllTextAsync(uiRetainedLogPath);
            var hostText = await File.ReadAllTextAsync(hostRetainedLogPath);
            Assert.Contains("old ui log", uiText, StringComparison.Ordinal);
            Assert.Contains("new ui log", uiText, StringComparison.Ordinal);
            Assert.Contains("old host log", hostText, StringComparison.Ordinal);
            Assert.Contains("new host log", hostText, StringComparison.Ordinal);
            Assert.False(File.Exists(uiRuntimeLogPath));
            Assert.False(File.Exists(hostRuntimeLogPath));
        }
        finally
        {
            TryDeleteDirectory(sandboxRoot);
        }
    }

    [Fact]
    public async Task SyncPendingRuntimeLogs_LogsWarningWhenRuntimeLogDeleteFails()
    {
        if (!OperatingSystem.IsWindows())
        {
            return;
        }

        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-tests",
            $"{Guid.NewGuid():N}");
        var libraryRoot = Path.Combine(sandboxRoot, "Library");
        Directory.CreateDirectory(Path.Combine(libraryRoot, "Logs"));
        var testLogPath = Path.Combine(sandboxRoot, "test-ui.log");

        try
        {
            UiLogging.ReinitializeForTests(testLogPath);
            var uiRuntimeLogPath = RuntimeEnvironment.GetUiRuntimeLogFilePathForLibrary(libraryRoot);
            Directory.CreateDirectory(Path.GetDirectoryName(uiRuntimeLogPath)!);
            await File.WriteAllTextAsync(uiRuntimeLogPath, "ui runtime log");

            using (new FileStream(uiRuntimeLogPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
            {
                RuntimeLogSynchronization.SyncPendingRuntimeLogs(libraryRoot);
            }

            UiLogging.Shutdown();
            var logText = await File.ReadAllTextAsync(testLogPath);
            Assert.Contains("Failed to delete runtime log after sync", logText, StringComparison.Ordinal);
        }
        finally
        {
            UiLogging.Shutdown();
            TryDeleteDirectory(sandboxRoot);
        }
    }

    private static void TryDeleteDirectory(string path)
    {
        try
        {
            Directory.Delete(path, recursive: true);
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }
}
