using System.ComponentModel;

namespace Librova.UI.ViewModels;

internal sealed class FilterFacetItem : INotifyPropertyChanged
{
    private bool _isSelected;
    private uint _bookCount;

    public string Value { get; }

    public uint BookCount
    {
        get => _bookCount;
        set
        {
            if (_bookCount == value)
                return;
            _bookCount = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(BookCount)));
        }
    }

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

    public FilterFacetItem(string value, uint bookCount = 0)
    {
        Value = value;
        _bookCount = bookCount;
    }
}
