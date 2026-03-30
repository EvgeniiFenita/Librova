using Librova.UI.CoreHost;
using Librova.UI.Logging;
using Librova.UI.Runtime;
using Librova.UI.Shell;
using Xunit;

namespace Librova.UI.Tests;

[Collection(EnvironmentVariableCollection.Name)]
public sealed class RuntimeEnvironmentTests
{
    [Fact]
    public void UiLogging_DefaultLogFilePath_UsesEnvironmentOverride()
    {
        var expected = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}", "ui.log");

        WithEnvironmentVariable(RuntimeEnvironment.UiLogFileEnvVar, expected, () =>
        {
            Assert.Equal(expected, UiLogging.DefaultLogFilePath);
        });
    }

    [Fact]
    public void ShellStateStore_CreateDefault_UsesEnvironmentOverride()
    {
        var expected = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}", "ui-shell-state.json");

        WithEnvironmentVariable(RuntimeEnvironment.UiStateFileEnvVar, expected, () =>
        {
            Assert.Equal(expected, ShellStateStore.CreateDefault().FilePath);
        });
    }

    [Fact]
    public void CoreHostDevelopmentDefaults_Create_UsesEnvironmentOverrides()
    {
        var expectedLibraryRoot = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}", "library");
        var expectedExecutable = Path.GetFullPath(Path.Combine(
            AppContext.BaseDirectory,
            "..",
            "..",
            "..",
            "..",
            "out",
            "build",
            "x64-debug",
            "apps",
            "Librova.Core.Host",
            "Debug",
            "LibrovaCoreHostApp.exe"));

        WithEnvironmentVariable(RuntimeEnvironment.LibraryRootEnvVar, expectedLibraryRoot, () =>
        {
            WithEnvironmentVariable(RuntimeEnvironment.CoreHostExecutableEnvVar, expectedExecutable, () =>
            {
                var options = CoreHostDevelopmentDefaults.Create();

                Assert.Equal(expectedLibraryRoot, options.LibraryRoot);
                Assert.Equal(expectedExecutable, options.ExecutablePath);
            });
        });
    }

    private static void WithEnvironmentVariable(string variableName, string value, Action action)
    {
        var previousValue = Environment.GetEnvironmentVariable(variableName);

        try
        {
            Environment.SetEnvironmentVariable(variableName, value);
            action();
        }
        finally
        {
            Environment.SetEnvironmentVariable(variableName, previousValue);
        }
    }
}
