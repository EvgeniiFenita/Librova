using Avalonia;

namespace Librova.UI;

internal static class Program
{
    [System.STAThread]
    private static void Main(string[] args)
    {
        Logging.UiLogging.EnsureInitialized();
        BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
    }

    public static AppBuilder BuildAvaloniaApp() =>
        AppBuilder.Configure<App>()
            .UsePlatformDetect()
            .LogToTrace();
}
