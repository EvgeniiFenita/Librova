using Librova.UI.Runtime;
using Xunit;

namespace Librova.UI.Tests;

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
