using System;
using System.IO;
using Librova.UI.Runtime;
using Librova.UI.Shell;
using Xunit;

namespace Librova.UI.Tests;

[Collection(EnvironmentVariableCollection.Name)]
public sealed class FirstRunSetupPolicyTests
{
    [Fact]
    public void RequiresSetup_TrueWhenNoPreferenceAndNoOverride()
    {
        WithLibraryRootOverride(null, () =>
        {
            Assert.True(FirstRunSetupPolicy.RequiresSetup(new FakePreferencesStore()));
        });
    }

    [Fact]
    public void RequiresSetup_FalseWhenPreferenceExists()
    {
        WithLibraryRootOverride(null, () =>
        {
            Assert.False(FirstRunSetupPolicy.RequiresSetup(new FakePreferencesStore
            {
                Snapshot = new UiPreferencesSnapshot
                {
                    PreferredLibraryRoot = @"D:\Librova\Data"
                }
            }));
        });
    }

    [Fact]
    public void RequiresSetup_FalseWhenOnlyPortablePreferenceExists()
    {
        WithLibraryRootOverride(null, () =>
        {
            Assert.False(FirstRunSetupPolicy.RequiresSetup(new FakePreferencesStore
            {
                Snapshot = new UiPreferencesSnapshot
                {
                    PortablePreferredLibraryRoot = "Library"
                }
            }));
        });
    }

    [Fact]
    public void RequiresSetup_FalseWhenLibraryRootOverrideExists()
    {
        WithLibraryRootOverride(@"E:\Librova\Override", () =>
        {
            Assert.False(FirstRunSetupPolicy.RequiresSetup(new FakePreferencesStore()));
        });
    }

    [Fact]
    public void BuildStartupRecoveryLibraryRootHint_IsNullWhenAbsoluteFallbackResolves()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-tests", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);
        File.WriteAllText(Path.Combine(sandboxRoot, "LibrovaCoreHostApp.exe"), string.Empty);

        try
        {
            Assert.Null(
                FirstRunSetupPolicy.BuildStartupRecoveryLibraryRootHint(
                    new FakePreferencesStore
                    {
                        Snapshot = new UiPreferencesSnapshot
                        {
                            PreferredLibraryRoot = @"E:\Detached\Librova"
                        }
                    },
                    sandboxRoot,
                    null));
        }
        finally
        {
            TryDeleteDirectory(sandboxRoot);
        }
    }

    [Fact]
    public void BuildStartupRecoveryLibraryRootHint_IsNullWhenNoSavedPreferenceExists()
    {
        WithLibraryRootOverride(null, () =>
        {
            Assert.Null(FirstRunSetupPolicy.BuildStartupRecoveryLibraryRootHint(new FakePreferencesStore()));
        });
    }

    [Fact]
    public void SavedPreferredLibraryRoot_CanBeValidatedBeforeHostStartup()
    {
        var tempFile = Path.GetTempFileName();
        try
        {
            var validation = LibraryRootValidation.BuildValidationMessage(tempFile);

            Assert.False(string.IsNullOrWhiteSpace(validation));
            Assert.Contains("must not point to a file", validation, StringComparison.OrdinalIgnoreCase);
        }
        finally
        {
            File.Delete(tempFile);
        }
    }

    private static void WithLibraryRootOverride(string? value, Action action)
    {
        var previous = Environment.GetEnvironmentVariable(RuntimeEnvironment.LibraryRootEnvVar);

        try
        {
            Environment.SetEnvironmentVariable(RuntimeEnvironment.LibraryRootEnvVar, value);
            action();
        }
        finally
        {
            Environment.SetEnvironmentVariable(RuntimeEnvironment.LibraryRootEnvVar, previous);
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

    private sealed class FakePreferencesStore : IUiPreferencesStore
    {
        public UiPreferencesSnapshot? Snapshot { get; init; }

        public UiPreferencesSnapshot? TryLoad() => Snapshot;

        public void Save(UiPreferencesSnapshot snapshot)
        {
        }

        public void Clear()
        {
        }
    }
}

