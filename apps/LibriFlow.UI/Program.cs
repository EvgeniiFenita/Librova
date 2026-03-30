namespace LibriFlow.UI;

internal static class Program
{
    [System.STAThread]
    private static void Main()
    {
        _ = CoreHost.CoreHostDevelopmentDefaults.Create();
    }
}
