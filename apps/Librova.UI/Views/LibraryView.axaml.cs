using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace Librova.UI.Views;

internal sealed partial class LibraryView : UserControl
{
    public LibraryView()
    {
        AvaloniaXamlLoader.Load(this);
    }
}
