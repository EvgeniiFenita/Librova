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
    public void RequiresSetup_FalseWhenLibraryRootOverrideExists()
    {
        WithLibraryRootOverride(@"E:\Librova\Override", () =>
        {
            Assert.False(FirstRunSetupPolicy.RequiresSetup(new FakePreferencesStore()));
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
