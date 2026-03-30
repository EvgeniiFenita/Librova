using Xunit;

namespace Librova.UI.Tests;

[CollectionDefinition(Name, DisableParallelization = true)]
public sealed class EnvironmentVariableCollection : ICollectionFixture<EnvironmentVariableFixture>
{
    public const string Name = "EnvironmentVariables";
}

public sealed class EnvironmentVariableFixture
{
}
