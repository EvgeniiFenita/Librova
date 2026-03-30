namespace Librova.UI.Shell;

internal interface IUiPreferencesStore
{
    UiPreferencesSnapshot? TryLoad();
    void Save(UiPreferencesSnapshot snapshot);
    void Clear();
}
