namespace Librova.UI.ViewModels;

internal enum EmptyStateKind
{
    None,          // books visible, or pending refresh – empty state panel is hidden
    LibraryEmpty,  // 0 books, no active filters
    NoResults      // 0 books, filters or search term active
}
