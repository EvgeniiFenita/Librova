using System;
using System.Diagnostics;
using System.IO.Pipes;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace LibriFlow.UI.CoreHost;

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
            throw new InvalidOperationException("Failed to start LibriFlow core host process.");
        }

        try
        {
            await WaitForPipeReadyAsync(options.PipePath, cancellationToken).ConfigureAwait(false);
            _process = process;
        }
        catch
        {
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

        TryTerminate(process);
        await process.WaitForExitAsync().ConfigureAwait(false);
    }

    private static string BuildArguments(CoreHostLaunchOptions options)
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

        return builder.ToString();
    }

    private static string Quote(string value) => "\"" + value + "\"";

    private static async Task WaitForPipeReadyAsync(string pipePath, CancellationToken cancellationToken)
    {
        var pipeName = pipePath.Replace(@"\\.\pipe\", string.Empty, StringComparison.Ordinal);
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(10);

        while (DateTime.UtcNow < deadline)
        {
            cancellationToken.ThrowIfCancellationRequested();

            try
            {
                await using var pipe = new NamedPipeClientStream(
                    ".",
                    pipeName,
                    PipeDirection.InOut,
                    PipeOptions.Asynchronous);

                await pipe.ConnectAsync(100, cancellationToken).ConfigureAwait(false);
                return;
            }
            catch (TimeoutException)
            {
            }
            catch (IOException)
            {
            }

            await Task.Delay(50, cancellationToken).ConfigureAwait(false);
        }

        throw new TimeoutException("Timed out waiting for LibriFlow core host pipe readiness.");
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
}
