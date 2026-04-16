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
    public void RuntimeEnvironment_GetUiLogFilePathForLibrary_UsesLibraryLogsDirectory()
    {
        var libraryRoot = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}", "library");

        var path = RuntimeEnvironment.GetUiLogFilePathForLibrary(libraryRoot);

        Assert.Equal(Path.Combine(libraryRoot, "Logs", "ui.log"), path);
    }

    [Fact]
    public void RuntimeEnvironment_GetRuntimeLogFilePathsForLibrary_UseLocalAppDataOutsidePortableMode()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}");
        var libraryRoot = Path.Combine(sandboxRoot, "library");
        var expectedRoot = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "Librova",
            "RuntimeLogs");

        var uiPath = RuntimeEnvironment.GetUiRuntimeLogFilePathForLibrary(libraryRoot, sandboxRoot, null);
        var hostPath = RuntimeEnvironment.GetHostRuntimeLogFilePathForLibrary(libraryRoot, sandboxRoot, null);

        Assert.StartsWith(expectedRoot, uiPath, StringComparison.OrdinalIgnoreCase);
        Assert.StartsWith(expectedRoot, hostPath, StringComparison.OrdinalIgnoreCase);
        Assert.EndsWith(Path.Combine("ui.log"), uiPath, StringComparison.OrdinalIgnoreCase);
        Assert.EndsWith(Path.Combine("host.log"), hostPath, StringComparison.OrdinalIgnoreCase);
        Assert.Equal(Path.GetDirectoryName(uiPath), Path.GetDirectoryName(hostPath));
    }

    [Fact]
    public void RuntimeEnvironment_GetRuntimeLogFilePathsForLibrary_UsePortableDataInPortableMode()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}");
        var libraryRoot = Path.Combine(sandboxRoot, "Library");
        Directory.CreateDirectory(sandboxRoot);
        File.WriteAllText(Path.Combine(sandboxRoot, "LibrovaCoreHostApp.exe"), string.Empty);

        try
        {
            var uiPath = RuntimeEnvironment.GetUiRuntimeLogFilePathForLibrary(libraryRoot, sandboxRoot, null);
            var hostPath = RuntimeEnvironment.GetHostRuntimeLogFilePathForLibrary(libraryRoot, sandboxRoot, null);

            Assert.StartsWith(
                Path.Combine(sandboxRoot, "PortableData", "RuntimeLogs"),
                uiPath,
                StringComparison.OrdinalIgnoreCase);
            Assert.StartsWith(
                Path.Combine(sandboxRoot, "PortableData", "RuntimeLogs"),
                hostPath,
                StringComparison.OrdinalIgnoreCase);
            Assert.EndsWith(Path.Combine("ui.log"), uiPath, StringComparison.OrdinalIgnoreCase);
            Assert.EndsWith(Path.Combine("host.log"), hostPath, StringComparison.OrdinalIgnoreCase);
        }
        finally
        {
            TryDeleteDirectory(sandboxRoot);
        }
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
    public void UiPreferencesStore_CreateDefault_UsesEnvironmentOverride()
    {
        var expected = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}", "ui-preferences.json");

        WithEnvironmentVariable(RuntimeEnvironment.UiPreferencesFileEnvVar, expected, () =>
        {
            Assert.Equal(expected, UiPreferencesStore.CreateDefault().FilePath);
        });
    }

    [Fact]
    public void RuntimeEnvironment_IsPortableMode_WhenPackagedHostExistsBesideBaseDirectory()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);
        var packagedHostPath = Path.Combine(sandboxRoot, "LibrovaCoreHostApp.exe");
        File.WriteAllText(packagedHostPath, string.Empty);

        try
        {
            Assert.True(RuntimeEnvironment.IsPortableMode(sandboxRoot, null));
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

    [Fact]
    public void RuntimeEnvironment_IsPortableMode_IsFalseWhenHostOverrideIsSet()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);
        var packagedHostPath = Path.Combine(sandboxRoot, "LibrovaCoreHostApp.exe");
        File.WriteAllText(packagedHostPath, string.Empty);

        try
        {
            Assert.False(RuntimeEnvironment.IsPortableMode(sandboxRoot, @"C:\Override\LibrovaCoreHostApp.exe"));
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

    [Fact]
    public void RuntimeEnvironment_DefaultPaths_UsePortableDataWhenPackagedHostExists()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);
        File.WriteAllText(Path.Combine(sandboxRoot, "LibrovaCoreHostApp.exe"), string.Empty);

        try
        {
            Assert.Equal(
                Path.Combine(sandboxRoot, "PortableData", "Logs", "ui.log"),
                RuntimeEnvironment.GetDefaultUiLogFilePath(sandboxRoot, null));
            Assert.Equal(
                Path.Combine(sandboxRoot, "PortableData", "ui-shell-state.json"),
                RuntimeEnvironment.GetDefaultUiStateFilePath(sandboxRoot, null));
            Assert.Equal(
                Path.Combine(sandboxRoot, "PortableData", "ui-preferences.json"),
                RuntimeEnvironment.GetDefaultUiPreferencesFilePath(sandboxRoot, null));
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

    [Fact]
    public void RuntimeEnvironment_BuildPortableLibraryRootPreference_UsesRelativePathInPortableMode()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}");
        var libraryRoot = Path.Combine(sandboxRoot, "Library");
        Directory.CreateDirectory(sandboxRoot);
        Directory.CreateDirectory(libraryRoot);
        File.WriteAllText(Path.Combine(sandboxRoot, "LibrovaCoreHostApp.exe"), string.Empty);

        try
        {
            var relativePath = RuntimeEnvironment.BuildPortableLibraryRootPreference(libraryRoot, sandboxRoot, null);
            Assert.Equal("Library", relativePath);
        }
        finally
        {
            TryDeleteDirectory(sandboxRoot);
        }
    }

    [Fact]
    public void RuntimeEnvironment_ResolvePreferredLibraryRoot_IgnoresPortableEscapeAndFallsBackToAbsolutePath()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);
        File.WriteAllText(Path.Combine(sandboxRoot, "LibrovaCoreHostApp.exe"), string.Empty);
        var expectedLibraryRoot = @"D:\Stale\Librova";

        try
        {
            var resolved = RuntimeEnvironment.ResolvePreferredLibraryRoot(
                expectedLibraryRoot,
                Path.Combine("..", "PortableLibrary"),
                sandboxRoot,
                null);

            Assert.Equal(expectedLibraryRoot, resolved);
        }
        finally
        {
            TryDeleteDirectory(sandboxRoot);
        }
    }

    [Fact]
    public void RuntimeEnvironment_ResolvePreferredLibraryRoot_FallsBackToAbsolutePathWhenPortableRelativeRootIsMissing()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}");
        var expectedLibraryRoot = @"D:\Librova\PortableLibrary";
        Directory.CreateDirectory(sandboxRoot);
        File.WriteAllText(Path.Combine(sandboxRoot, "LibrovaCoreHostApp.exe"), string.Empty);

        try
        {
            var resolved = RuntimeEnvironment.ResolvePreferredLibraryRoot(
                expectedLibraryRoot,
                "Library",
                sandboxRoot,
                null);

            Assert.Equal(expectedLibraryRoot, resolved);
        }
        finally
        {
            TryDeleteDirectory(sandboxRoot);
        }
    }

    [Fact]
    public void RuntimeEnvironment_ResolvePreferredLibraryRoot_IgnoresAbsolutePortableHintAndFallsBackToPreferredRoot()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}");
        var expectedLibraryRoot = @"D:\Librova\PreferredLibrary";
        Directory.CreateDirectory(sandboxRoot);
        File.WriteAllText(Path.Combine(sandboxRoot, "LibrovaCoreHostApp.exe"), string.Empty);

        try
        {
            var resolved = RuntimeEnvironment.ResolvePreferredLibraryRoot(
                expectedLibraryRoot,
                @"D:\Librova\BadPortableHint",
                sandboxRoot,
                null);

            Assert.Equal(expectedLibraryRoot, resolved);
        }
        finally
        {
            TryDeleteDirectory(sandboxRoot);
        }
    }

    [Fact]
    public void RuntimeEnvironment_ResolvePreferredLibraryRoot_AllowsAbsolutePortableRootInsideBaseDirectory()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}");
        var libraryRoot = Path.Combine(sandboxRoot, "Library");
        Directory.CreateDirectory(libraryRoot);
        File.WriteAllText(Path.Combine(sandboxRoot, "LibrovaCoreHostApp.exe"), string.Empty);

        try
        {
            var resolved = RuntimeEnvironment.ResolvePreferredLibraryRoot(
                libraryRoot,
                null,
                sandboxRoot,
                null);

            Assert.Equal(libraryRoot, resolved);
        }
        finally
        {
            TryDeleteDirectory(sandboxRoot);
        }
    }

    [Fact]
    public void RuntimeEnvironment_BuildPortableLibraryRootPreference_RejectsPathsOutsidePortableBaseDirectory()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}");
        var outsideLibraryRoot = Path.GetFullPath(Path.Combine(sandboxRoot, "..", "PortableLibrary"));
        Directory.CreateDirectory(sandboxRoot);
        Directory.CreateDirectory(outsideLibraryRoot);
        File.WriteAllText(Path.Combine(sandboxRoot, "LibrovaCoreHostApp.exe"), string.Empty);

        try
        {
            var relativePath = RuntimeEnvironment.BuildPortableLibraryRootPreference(outsideLibraryRoot, sandboxRoot, null);
            Assert.Null(relativePath);
        }
        finally
        {
            TryDeleteDirectory(sandboxRoot);
            TryDeleteDirectory(outsideLibraryRoot);
        }
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
                Assert.Equal(
                    RuntimeEnvironment.GetHostRuntimeLogFilePathForLibrary(expectedLibraryRoot),
                    options.HostLogFilePath);
            });
        });
    }

    [Fact]
    public void CoreHostDevelopmentDefaults_Create_UsesSavedPreferredLibraryRootWhenNoEnvironmentOverrideExists()
    {
        var preferencesFile = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}", "ui-preferences.json");
        var expectedLibraryRoot = @"D:\Librova\PreferredLibrary";
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

        Directory.CreateDirectory(Path.GetDirectoryName(preferencesFile)!);
        new UiPreferencesStore(preferencesFile).Save(new UiPreferencesSnapshot
        {
            PreferredLibraryRoot = expectedLibraryRoot
        });

        WithEnvironmentVariable(RuntimeEnvironment.UiPreferencesFileEnvVar, preferencesFile, () =>
        {
            WithEnvironmentVariable(RuntimeEnvironment.LibraryRootEnvVar, null, () =>
            {
                WithEnvironmentVariable(RuntimeEnvironment.CoreHostExecutableEnvVar, expectedExecutable, () =>
                {
                    var options = CoreHostDevelopmentDefaults.Create();
                    Assert.Equal(expectedLibraryRoot, options.LibraryRoot);
                });
            });
        });
    }

    [Fact]
    public void CoreHostDevelopmentDefaults_Create_PrefersPortableRelativeLibraryRootWhenPortableModeMoves()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}");
        var preferencesFile = Path.Combine(sandboxRoot, "PortableData", "ui-preferences.json");
        var portableLibraryRoot = Path.Combine(sandboxRoot, "Library");
        var staleAbsoluteLibraryRoot = @"D:\Librova\PreferredLibrary";
        var packagedHostPath = Path.Combine(sandboxRoot, "LibrovaCoreHostApp.exe");

        Directory.CreateDirectory(Path.GetDirectoryName(preferencesFile)!);
        Directory.CreateDirectory(portableLibraryRoot);
        File.WriteAllText(packagedHostPath, string.Empty);
        new UiPreferencesStore(preferencesFile).Save(new UiPreferencesSnapshot
        {
            PreferredLibraryRoot = staleAbsoluteLibraryRoot,
            PortablePreferredLibraryRoot = "Library"
        });

        try
        {
            WithEnvironmentVariable(RuntimeEnvironment.CoreHostExecutableEnvVar, null, () =>
            {
                WithEnvironmentVariable(RuntimeEnvironment.UiPreferencesFileEnvVar, preferencesFile, () =>
                {
                    WithEnvironmentVariable(RuntimeEnvironment.LibraryRootEnvVar, null, () =>
                    {
                        var options = CoreHostDevelopmentDefaults.Create(baseDirectory: sandboxRoot);
                        Assert.Equal(portableLibraryRoot, options.LibraryRoot);
                        Assert.Equal(packagedHostPath, options.ExecutablePath);
                        Assert.StartsWith(
                            Path.Combine(sandboxRoot, "PortableData", "RuntimeLogs"),
                            options.HostLogFilePath,
                            StringComparison.OrdinalIgnoreCase);
                    });
                });
            });
        }
        finally
        {
            TryDeleteDirectory(sandboxRoot);
        }
    }

    [Fact]
    public void CoreHostDevelopmentDefaults_Create_DoesNotSurfaceLegacyFb2CngConfigFromPreferences()
    {
        var preferencesFile = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}", "ui-preferences.json");
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

        Directory.CreateDirectory(Path.GetDirectoryName(preferencesFile)!);
        new UiPreferencesStore(preferencesFile).Save(new UiPreferencesSnapshot
        {
            PreferredLibraryRoot = @"D:\Librova\PreferredLibrary",
            ConverterMode = UiConverterMode.BuiltInFb2Cng,
            Fb2CngExecutablePath = @"C:\Tools\fbc.exe",
            Fb2CngConfigPath = @"C:\Tools\fbc.yaml"
        });

        WithEnvironmentVariable(RuntimeEnvironment.UiPreferencesFileEnvVar, preferencesFile, () =>
        {
            WithEnvironmentVariable(RuntimeEnvironment.LibraryRootEnvVar, null, () =>
            {
                WithEnvironmentVariable(RuntimeEnvironment.CoreHostExecutableEnvVar, expectedExecutable, () =>
                {
                    var options = CoreHostDevelopmentDefaults.Create();
                    Assert.Equal(@"C:\Tools\fbc.exe", options.Fb2CngExecutablePath);
                    Assert.Null(options.Fb2CngConfigPath);
                });
            });
        });
    }

    [Fact]
    public void CoreHostDevelopmentDefaults_Create_DisablesConverterWhenLegacyModeHasNoExecutablePath()
    {
        var preferencesFile = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}", "ui-preferences.json");
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

        Directory.CreateDirectory(Path.GetDirectoryName(preferencesFile)!);
        new UiPreferencesStore(preferencesFile).Save(new UiPreferencesSnapshot
        {
            PreferredLibraryRoot = @"D:\Librova\PreferredLibrary",
            ConverterMode = UiConverterMode.BuiltInFb2Cng,
            ForceEpubConversionOnImport = true
        });

        WithEnvironmentVariable(RuntimeEnvironment.UiPreferencesFileEnvVar, preferencesFile, () =>
        {
            WithEnvironmentVariable(RuntimeEnvironment.LibraryRootEnvVar, null, () =>
            {
                WithEnvironmentVariable(RuntimeEnvironment.CoreHostExecutableEnvVar, expectedExecutable, () =>
                {
                    var options = CoreHostDevelopmentDefaults.Create();
                    Assert.Equal(UiConverterMode.Disabled, options.ConverterMode);
                    Assert.Null(options.Fb2CngExecutablePath);
                });
            });
        });
    }

    private static void WithEnvironmentVariable(string variableName, string? value, Action action)
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

    private static void TryDeleteDirectory(string path)
    {
        try
        {
            if (Directory.Exists(path))
            {
                Directory.Delete(path, true);
            }
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }
}
