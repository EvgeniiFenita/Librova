using Librova.UI.Runtime;
using Xunit;

namespace Librova.UI.Tests;

public sealed class RuntimeWorkspaceMaintenanceTests
{
    [Fact]
    public void PrepareForSession_RemovesOwnedRuntimeWorkspaceDirectoriesAndKeepsUnrelatedChildren()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-runtime-maintenance",
            $"{Guid.NewGuid():N}");
        var libraryRoot = Path.Combine(sandboxRoot, "Library");
        Directory.CreateDirectory(libraryRoot);

        try
        {
            var runtimeRoot = RuntimeEnvironment.GetRuntimeWorkspaceRootForLibrary(
                libraryRoot,
                sandboxRoot,
                @"C:\Override\LibrovaCoreHostApp.exe");
            var generatedImportWorkspace = Path.Combine(runtimeRoot, "ImportWorkspaces", "GeneratedUiImportWorkspace");
            var converterWorkspace = Path.Combine(runtimeRoot, "ConverterWorkspace");
            var managedStorageStaging = Path.Combine(runtimeRoot, "ManagedStorageStaging");
            var runtimeLogs = Path.Combine(runtimeRoot, "RuntimeLogs");
            var unrelatedDirectory = Path.Combine(runtimeRoot, "KeepMe");

            Directory.CreateDirectory(generatedImportWorkspace);
            Directory.CreateDirectory(converterWorkspace);
            Directory.CreateDirectory(managedStorageStaging);
            Directory.CreateDirectory(runtimeLogs);
            Directory.CreateDirectory(unrelatedDirectory);
            File.WriteAllText(Path.Combine(generatedImportWorkspace, "entry.tmp"), "import");
            File.WriteAllText(Path.Combine(converterWorkspace, "converter.tmp"), "converter");
            File.WriteAllText(Path.Combine(managedStorageStaging, "book.tmp"), "staging");
            File.WriteAllText(Path.Combine(runtimeLogs, "ui.log"), "log");
            File.WriteAllText(Path.Combine(unrelatedDirectory, "keep.txt"), "keep");

            RuntimeWorkspaceMaintenance.PrepareForSession(
                libraryRoot,
                sandboxRoot,
                @"C:\Override\LibrovaCoreHostApp.exe");

            Assert.False(Directory.Exists(generatedImportWorkspace));
            Assert.False(Directory.Exists(converterWorkspace));
            Assert.False(Directory.Exists(managedStorageStaging));
            Assert.False(Directory.Exists(runtimeLogs));
            Assert.False(Directory.Exists(Path.Combine(runtimeRoot, "ImportWorkspaces")));
            Assert.True(Directory.Exists(unrelatedDirectory));
            Assert.True(File.Exists(Path.Combine(unrelatedDirectory, "keep.txt")));
        }
        finally
        {
            TryDeleteDirectory(sandboxRoot);
        }
    }

    [Fact]
    public void PrepareForSession_RemovesRuntimeRootWhenOnlyOwnedDirectoriesExist()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-runtime-maintenance-root",
            $"{Guid.NewGuid():N}");
        var libraryRoot = Path.Combine(sandboxRoot, "Library");
        Directory.CreateDirectory(libraryRoot);

        try
        {
            var runtimeRoot = RuntimeEnvironment.GetRuntimeWorkspaceRootForLibrary(
                libraryRoot,
                sandboxRoot,
                @"C:\Override\LibrovaCoreHostApp.exe");
            Directory.CreateDirectory(Path.Combine(runtimeRoot, "ImportWorkspaces", "GeneratedUiImportWorkspace"));
            File.WriteAllText(Path.Combine(runtimeRoot, "ImportWorkspaces", "GeneratedUiImportWorkspace", "entry.tmp"), "import");

            RuntimeWorkspaceMaintenance.PrepareForSession(
                libraryRoot,
                sandboxRoot,
                @"C:\Override\LibrovaCoreHostApp.exe");

            Assert.False(Directory.Exists(runtimeRoot));
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

