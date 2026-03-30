using Avalonia;

namespace LibriFlow.UI;

internal static class Program
{
    [System.STAThread]
    private static void Main()
    {
        Logging.UiLogging.EnsureInitialized();
        BuildAvaloniaApp().StartWithClassicDesktopLifetime(Array.Empty<string>());
    }

    public static AppBuilder BuildAvaloniaApp() =>
        AppBuilder.Configure<App>()
            .UsePlatformDetect()
            .LogToTrace();
}
