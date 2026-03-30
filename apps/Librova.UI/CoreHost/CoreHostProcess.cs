using System;
using System.Diagnostics;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Librova.UI.Logging;
using System.Runtime.InteropServices;

namespace Librova.UI.CoreHost;

internal sealed class CoreHostProcess : IAsyncDisposable
{
    private Process? _process;

    public async Task StartAsync(CoreHostLaunchOptions options, CancellationToken cancellationToken)
    {
        options.Validate();

        if (_process is not null)
        {
            throw new InvalidOperationException("Core host process is already running.");
        }

        UiLogging.Information(
            "Starting core host process. ExecutablePath={ExecutablePath} PipePath={PipePath} LibraryRoot={LibraryRoot}",
            options.ExecutablePath,
            options.PipePath,
            options.LibraryRoot);

        var startInfo = new ProcessStartInfo
        {
            FileName = options.ExecutablePath,
            Arguments = BuildArguments(options),
            UseShellExecute = false,
            CreateNoWindow = true
        };

        var process = new Process
        {
            StartInfo = startInfo,
            EnableRaisingEvents = true
        };

        if (!process.Start())
        {
            process.Dispose();
            throw new InvalidOperationException("Failed to start Librova core host process.");
        }

        try
        {
            await WaitForPipeReadyAsync(options.PipePath, cancellationToken).ConfigureAwait(false);
            _process = process;
            UiLogging.Information(
                "Core host process is ready. ProcessId={ProcessId} PipePath={PipePath}",
                process.Id,
                options.PipePath);
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Failed to start or initialize core host process.");
            TryTerminate(process);
            process.Dispose();
            throw;
        }
    }

    public async ValueTask DisposeAsync()
    {
        if (_process is null)
        {
            return;
        }

        using var process = _process;
        _process = null;

        UiLogging.Information("Stopping core host process. ProcessId={ProcessId}", process.Id);
        TryTerminate(process);
        await process.WaitForExitAsync().ConfigureAwait(false);
        UiLogging.Information("Core host process stopped. ProcessId={ProcessId}", process.Id);
    }

    internal static string BuildArguments(CoreHostLaunchOptions options)
    {
        var builder = new StringBuilder();
        builder.Append("--pipe ").Append(Quote(options.PipePath));
        builder.Append(" --library-root ").Append(Quote(options.LibraryRoot));

        if (options.ServeOneSession)
        {
            builder.Append(" --serve-one");
        }

        if (options.MaxSessions.HasValue)
        {
            builder.Append(" --max-sessions ").Append(options.MaxSessions.Value);
        }

        switch (options.ConverterMode)
        {
            case UiConverterMode.BuiltInFb2Cng:
                builder.Append(" --fb2cng-exe ").Append(Quote(options.Fb2CngExecutablePath!));
                if (!string.IsNullOrWhiteSpace(options.Fb2CngConfigPath))
                {
                    builder.Append(" --fb2cng-config ").Append(Quote(options.Fb2CngConfigPath));
                }
                break;

            case UiConverterMode.CustomCommand:
                builder.Append(" --converter-exe ").Append(Quote(options.CustomConverterExecutablePath!));
                foreach (var argument in options.CustomConverterArguments)
                {
                    builder.Append(" --converter-arg ").Append(Quote(argument));
                }

                builder.Append(" --converter-output ")
                    .Append(options.CustomConverterOutputMode is UiConverterOutputMode.SingleFileInDestinationDirectory
                        ? "directory"
                        : "exact");
                break;
        }

        return builder.ToString();
    }

    private static string Quote(string value) => "\"" + value + "\"";

    private static async Task WaitForPipeReadyAsync(string pipePath, CancellationToken cancellationToken)
    {
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(10);

        while (DateTime.UtcNow < deadline)
        {
            cancellationToken.ThrowIfCancellationRequested();

            if (WaitNamedPipe(pipePath, 100))
            {
                return;
            }

            await Task.Delay(50, cancellationToken).ConfigureAwait(false);
        }

        throw new TimeoutException("Timed out waiting for Librova core host pipe readiness.");
    }

    private static void TryTerminate(Process process)
    {
        try
        {
            if (!process.HasExited)
            {
                process.Kill(entireProcessTree: true);
            }
        }
        catch (InvalidOperationException)
        {
        }
    }

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool WaitNamedPipe(string name, int timeout);
}
