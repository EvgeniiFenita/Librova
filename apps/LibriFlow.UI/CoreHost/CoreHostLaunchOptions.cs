using System;
using System.IO;

namespace LibriFlow.UI.CoreHost;

internal sealed class CoreHostLaunchOptions
{
    public required string ExecutablePath { get; init; }
    public required string PipePath { get; init; }
    public required string LibraryRoot { get; init; }
    public bool ServeOneSession { get; init; }
    public int? MaxSessions { get; init; }

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

        if (MaxSessions is <= 0)
        {
            throw new InvalidOperationException("MaxSessions must be positive when provided.");
        }
    }
}
