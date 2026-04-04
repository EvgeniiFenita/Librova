using System.Reflection;

namespace Librova.UI.Runtime;

internal static class ApplicationVersion
{
    public static string Value { get; } = ReadVersion();

    private static string ReadVersion()
    {
        var assembly = typeof(ApplicationVersion).Assembly;
        var informationalVersion = assembly
            .GetCustomAttribute<AssemblyInformationalVersionAttribute>()?
            .InformationalVersion;

        if (!string.IsNullOrWhiteSpace(informationalVersion))
        {
            var separatorIndex = informationalVersion.IndexOf('+');
            return separatorIndex >= 0
                ? informationalVersion[..separatorIndex]
                : informationalVersion;
        }

        var assemblyVersion = assembly.GetName().Version?.ToString();
        return string.IsNullOrWhiteSpace(assemblyVersion) ? "0.0.0" : assemblyVersion;
    }
}
