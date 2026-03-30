using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace Librova.UI.Views;

internal sealed partial class SettingsView : UserControl
{
    public SettingsView()
    {
        AvaloniaXamlLoader.Load(this);
    }
}
