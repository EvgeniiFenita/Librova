namespace Librova.UI.Shell;

internal sealed class ShellStateSnapshot
{
    public string[]? SourcePaths { get; init; }
    public string? WorkingDirectory { get; init; }
    public bool AllowProbableDuplicates { get; init; }
}
