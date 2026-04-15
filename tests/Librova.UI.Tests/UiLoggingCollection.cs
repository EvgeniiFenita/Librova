using Xunit;

namespace Librova.UI.Tests;

[CollectionDefinition(Name, DisableParallelization = true)]
public sealed class UiLoggingCollection : ICollectionFixture<UiLoggingFixture>
{
    public const string Name = "UiLogging";
}

public sealed class UiLoggingFixture
{
}
