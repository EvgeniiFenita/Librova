using Librova.UI.Logging;
using Librova.UI.Runtime;
using System;
using System.IO;
using System.Text.Json;

namespace Librova.UI.Shell;

internal sealed class UiPreferencesStore : IUiPreferencesStore
{
    private static readonly JsonSerializerOptions SerializerOptions = new()
    {
        WriteIndented = true
    };

    public UiPreferencesStore(string filePath)
    {
        FilePath = filePath;
    }

    public string FilePath { get; }

    public static UiPreferencesStore CreateDefault() =>
        new(RuntimeEnvironment.GetDefaultUiPreferencesFilePath());

    public UiPreferencesSnapshot? TryLoad()
    {
        try
        {
            if (!File.Exists(FilePath))
            {
                return null;
            }

            var json = File.ReadAllText(FilePath);
            return JsonSerializer.Deserialize<UiPreferencesSnapshot>(json, SerializerOptions);
        }
        catch (Exception error)
        {
            UiLogging.Warning("Failed to load UI preferences from {FilePath}: {Error}", FilePath, error.Message);
            return null;
        }
    }

    public void Save(UiPreferencesSnapshot snapshot)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
        var json = JsonSerializer.Serialize(snapshot, SerializerOptions);
        File.WriteAllText(FilePath, json);
    }

    public void Clear()
    {
        if (File.Exists(FilePath))
        {
            File.Delete(FilePath);
        }
    }
}
