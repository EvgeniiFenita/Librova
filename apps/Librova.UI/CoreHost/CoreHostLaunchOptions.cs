using System;
using System.IO;

namespace Librova.UI.CoreHost;

internal sealed class CoreHostLaunchOptions
{
    public required string ExecutablePath { get; init; }
    public required string PipePath { get; init; }
    public required string LibraryRoot { get; init; }
    public string? HostLogFilePath { get; init; }
    public string? ShutdownEventName { get; init; }
    public UiLibraryOpenMode LibraryOpenMode { get; init; } = UiLibraryOpenMode.OpenExisting;
    public int? ParentProcessId { get; init; }
    public bool ServeOneSession { get; init; }
    public int? MaxSessions { get; init; }
    public UiConverterMode ConverterMode { get; init; }
    public string? Fb2CngExecutablePath { get; init; }
    public string? Fb2CngConfigPath { get; init; }

    public void Validate()
    {
        if (string.IsNullOrWhiteSpace(ExecutablePath))
        {
            throw new InvalidOperationException("Core host executable path must be provided.");
        }

        if (!Path.IsPathFullyQualified(ExecutablePath))
        {
            throw new InvalidOperationException("Core host executable path must be absolute.");
        }

        if (string.IsNullOrWhiteSpace(PipePath))
        {
            throw new InvalidOperationException("Pipe path must be provided.");
        }

        if (!PipePath.StartsWith(@"\\.\pipe\", StringComparison.Ordinal))
        {
            throw new InvalidOperationException("Pipe path must use the Windows named pipe syntax.");
        }

        if (string.IsNullOrWhiteSpace(LibraryRoot))
        {
            throw new InvalidOperationException("Library root must be provided.");
        }

        if (!Path.IsPathFullyQualified(LibraryRoot))
        {
            throw new InvalidOperationException("Library root path must be absolute.");
        }

        if (!string.IsNullOrWhiteSpace(HostLogFilePath) && !Path.IsPathFullyQualified(HostLogFilePath))
        {
            throw new InvalidOperationException("Host log file path must be absolute when provided.");
        }

        if (!Enum.IsDefined(LibraryOpenMode))
        {
            throw new InvalidOperationException("Library open mode must be valid.");
        }

        if (MaxSessions is <= 0)
        {
            throw new InvalidOperationException("MaxSessions must be positive when provided.");
        }

        if (ParentProcessId is <= 0)
        {
            throw new InvalidOperationException("ParentProcessId must be positive when provided.");
        }

        if (!Enum.IsDefined(ConverterMode))
        {
            throw new InvalidOperationException("Converter mode must be valid.");
        }

        switch (ConverterMode)
        {
            case UiConverterMode.Disabled:
                return;

            case UiConverterMode.BuiltInFb2Cng:
                if (string.IsNullOrWhiteSpace(Fb2CngExecutablePath))
                {
                    throw new InvalidOperationException("Built-in converter executable path must be provided.");
                }

                if (!Path.IsPathFullyQualified(Fb2CngExecutablePath))
                {
                    throw new InvalidOperationException("Built-in converter executable path must be absolute.");
                }

                if (!string.IsNullOrWhiteSpace(Fb2CngConfigPath) && !Path.IsPathFullyQualified(Fb2CngConfigPath))
                {
                    throw new InvalidOperationException("Built-in converter config path must be absolute.");
                }

                return;

            default:
                throw new InvalidOperationException("Converter mode must be valid.");
        }
    }
}
