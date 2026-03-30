using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace Librova.UI.Views;

internal sealed partial class MainWindow : Window
{
    public MainWindow()
    {
        AvaloniaXamlLoader.Load(this);
    }
}
