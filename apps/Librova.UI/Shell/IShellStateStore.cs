namespace Librova.UI.Shell;

internal interface IShellStateStore
{
    ShellStateSnapshot? TryLoad();
    void Save(ShellStateSnapshot snapshot);
}
