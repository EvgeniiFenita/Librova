namespace Librova.UI.Shell;

internal sealed class ShellLaunchOptions
{
    public string? InitialSourcePath { get; init; }

    public static ShellLaunchOptions FromArgs(string[]? args)
    {
        if (args is null || args.Length == 0)
        {
            return new ShellLaunchOptions();
        }

        foreach (var argument in args)
        {
            if (!string.IsNullOrWhiteSpace(argument))
            {
                return new ShellLaunchOptions
                {
                    InitialSourcePath = argument
                };
            }
        }

        return new ShellLaunchOptions();
    }
}
