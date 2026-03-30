using Librova.UI.Logging;
using Librova.UI.Runtime;
using System;
using System.IO;
using System.Text.Json;

namespace Librova.UI.Shell;

internal sealed class ShellStateStore : IShellStateStore
{
    private static readonly JsonSerializerOptions SerializerOptions = new()
    {
        WriteIndented = true
    };

    public ShellStateStore(string filePath)
    {
        FilePath = filePath;
    }

    public string FilePath { get; }

    public static ShellStateStore CreateDefault() =>
        new(RuntimeEnvironment.GetDefaultUiStateFilePath());

    public ShellStateSnapshot? TryLoad()
    {
        try
        {
            if (!File.Exists(FilePath))
            {
                return null;
            }

            var json = File.ReadAllText(FilePath);
            return JsonSerializer.Deserialize<ShellStateSnapshot>(json, SerializerOptions);
        }
        catch (Exception error)
        {
            UiLogging.Warning("Failed to load UI shell state from {FilePath}: {Error}", FilePath, error.Message);
            return null;
        }
    }

    public void Save(ShellStateSnapshot snapshot)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
        var json = JsonSerializer.Serialize(snapshot, SerializerOptions);
        File.WriteAllText(FilePath, json);
    }
}
