using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace Librova.UI.Views;

internal sealed partial class ImportView : UserControl
{
    public ImportView()
    {
        AvaloniaXamlLoader.Load(this);
    }
}
