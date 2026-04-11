using System.ComponentModel;

namespace Librova.UI.ViewModels;

internal sealed class FilterFacetItem : INotifyPropertyChanged
{
    private bool _isSelected;

    public string Value { get; }

    public bool IsSelected
    {
        get => _isSelected;
        set
        {
            if (_isSelected == value)
            {
                return;
            }

            _isSelected = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsSelected)));
        }
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    public FilterFacetItem(string value)
    {
        Value = value;
    }
}
