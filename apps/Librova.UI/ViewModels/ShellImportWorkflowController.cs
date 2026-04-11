using Librova.UI.ImportJobs;
using Librova.UI.LibraryCatalog;
using System;
using System.ComponentModel;
using System.Threading.Tasks;

namespace Librova.UI.ViewModels;

internal sealed class ShellImportWorkflowController
{
    private readonly ImportJobsViewModel _importJobs;
    private readonly LibraryBrowserViewModel _libraryBrowser;
    private readonly Func<ShellSection> _getCurrentSection;
    private readonly Action<ShellSection> _setCurrentSection;
    private readonly Action _raiseImportStateProperties;
    private readonly Action _raiseNavigationCanExecuteChanged;
    private readonly Action _raiseLibraryStatisticsChanged;
    private readonly Action<BookSortModel, bool> _saveSortPreference;

    public ShellImportWorkflowController(
        ImportJobsViewModel importJobs,
        LibraryBrowserViewModel libraryBrowser,
        Func<ShellSection> getCurrentSection,
        Action<ShellSection> setCurrentSection,
        Action raiseImportStateProperties,
        Action raiseNavigationCanExecuteChanged,
        Action raiseLibraryStatisticsChanged,
        Action<BookSortModel, bool> saveSortPreference)
    {
        _importJobs = importJobs;
        _libraryBrowser = libraryBrowser;
        _getCurrentSection = getCurrentSection;
        _setCurrentSection = setCurrentSection;
        _raiseImportStateProperties = raiseImportStateProperties;
        _raiseNavigationCanExecuteChanged = raiseNavigationCanExecuteChanged;
        _raiseLibraryStatisticsChanged = raiseLibraryStatisticsChanged;
        _saveSortPreference = saveSortPreference;
    }

    public Task HandleImportCompletedSuccessfullyAsync(ImportJobResultModel result) =>
        _libraryBrowser.RefreshAsync();

    public void HandleImportJobsPropertyChanged(object? sender, PropertyChangedEventArgs eventArgs)
    {
        if (eventArgs.PropertyName is not nameof(ImportJobsViewModel.IsBusy))
        {
            return;
        }

        _raiseImportStateProperties();
        if (_importJobs.IsBusy && _getCurrentSection() is not ShellSection.Import)
        {
            _setCurrentSection(ShellSection.Import);
        }

        _raiseNavigationCanExecuteChanged();
    }

    public void HandleLibraryBrowserPropertyChanged(object? sender, PropertyChangedEventArgs eventArgs)
    {
        if (eventArgs.PropertyName is nameof(LibraryBrowserViewModel.LibraryStatisticsText))
        {
            _raiseLibraryStatisticsChanged();
        }

        if (eventArgs.PropertyName is nameof(LibraryBrowserViewModel.SelectedSortKey)
            && _libraryBrowser.SelectedSortKey is not null)
        {
            _saveSortPreference(_libraryBrowser.SelectedSortKey.Key, _libraryBrowser.SortDescending);
        }

        if (eventArgs.PropertyName is nameof(LibraryBrowserViewModel.SortDescending)
            && _libraryBrowser.SelectedSortKey is not null)
        {
            _saveSortPreference(_libraryBrowser.SelectedSortKey.Key, _libraryBrowser.SortDescending);
        }
    }
}
