using System;
using System.Diagnostics;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Librova.UI.Logging;
using System.Runtime.InteropServices;
using System.Collections.Concurrent;
using Microsoft.Win32.SafeHandles;

namespace Librova.UI.CoreHost;

internal sealed class CoreHostProcess : IAsyncDisposable
{
    private Process? _process;
    private SafeKernelHandle? _jobHandle;
    private SafeKernelHandle? _shutdownEventHandle;

    public CoreHostProcess()
    {
    }

    internal CoreHostProcess(SafeKernelHandle? lifetimeJobHandle, SafeKernelHandle? shutdownEventHandle)
    {
        _jobHandle = lifetimeJobHandle;
        _shutdownEventHandle = shutdownEventHandle;
    }

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
            CreateNoWindow = true,
            RedirectStandardError = true,
            RedirectStandardOutput = true
        };

        var process = new Process
        {
            StartInfo = startInfo,
            EnableRaisingEvents = true
        };
        var startupOutput = new ConcurrentQueue<string>();
        process.OutputDataReceived += (_, eventArgs) => AppendStartupOutput(startupOutput, eventArgs.Data);
        process.ErrorDataReceived += (_, eventArgs) => AppendStartupOutput(startupOutput, eventArgs.Data);

        if (!process.Start())
        {
            process.Dispose();
            throw new InvalidOperationException("Failed to start Librova core host process.");
        }

        process.BeginOutputReadLine();
        process.BeginErrorReadLine();

        try
        {
            CreateShutdownEvent(options);
            AttachProcessToLifetimeJob(process);
            await WaitForPipeReadyAsync(process, options.PipePath, startupOutput, cancellationToken).ConfigureAwait(false);
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
            ReleaseLifetimeJob();
            ReleaseShutdownEvent();
            process.Dispose();
            throw;
        }
    }

    public async ValueTask DisposeAsync()
    {
        if (_process is null)
        {
            ReleaseLifetimeJob();
            ReleaseShutdownEvent();
            return;
        }

        using var process = _process;
        _process = null;

        UiLogging.Information("Stopping core host process. ProcessId={ProcessId}", process.Id);
        var exitedGracefully = await TryRequestGracefulShutdownAsync(process).ConfigureAwait(false);
        if (!exitedGracefully)
        {
            TryTerminate(process);
            await process.WaitForExitAsync().ConfigureAwait(false);
        }
        ReleaseLifetimeJob();
        ReleaseShutdownEvent();
        UiLogging.Information("Core host process stopped. ProcessId={ProcessId}", process.Id);
    }

    internal static string BuildArguments(CoreHostLaunchOptions options)
    {
        var builder = new StringBuilder();
        builder.Append("--pipe ").Append(Quote(options.PipePath));
        builder.Append(" --library-root ").Append(Quote(options.LibraryRoot));
        if (!string.IsNullOrWhiteSpace(options.HostLogFilePath))
        {
            builder.Append(" --log-file ").Append(Quote(options.HostLogFilePath));
        }
        if (!string.IsNullOrWhiteSpace(options.ConverterWorkingDirectory))
        {
            builder.Append(" --converter-working-dir ").Append(Quote(options.ConverterWorkingDirectory));
        }
        if (!string.IsNullOrWhiteSpace(options.ManagedStorageStagingRoot))
        {
            builder.Append(" --managed-storage-staging-root ").Append(Quote(options.ManagedStorageStagingRoot));
        }
        if (!string.IsNullOrWhiteSpace(options.ShutdownEventName))
        {
            builder.Append(" --shutdown-event ").Append(Quote(options.ShutdownEventName));
        }
        builder.Append(" --library-mode ").Append(options.LibraryOpenMode is UiLibraryOpenMode.CreateNew ? "create" : "open");
        if (options.ParentProcessId.HasValue)
        {
            builder.Append(" --parent-pid ").Append(options.ParentProcessId.Value);
            builder.Append(" --parent-start-unix-ms ").Append(options.ParentProcessCreatedAtUnixMs!.Value);
        }

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
        }

        return builder.ToString();
    }

    private static string Quote(string value)
    {
        if (string.IsNullOrEmpty(value))
        {
            return "\"\"";
        }

        var builder = new StringBuilder(value.Length + 2);
        builder.Append('"');
        var backslashCount = 0;

        foreach (var character in value)
        {
            if (character == '\\')
            {
                backslashCount++;
                continue;
            }

            if (character == '"')
            {
                builder.Append('\\', backslashCount * 2 + 1);
                builder.Append('"');
                backslashCount = 0;
                continue;
            }

            if (backslashCount > 0)
            {
                builder.Append('\\', backslashCount);
                backslashCount = 0;
            }

            builder.Append(character);
        }

        if (backslashCount > 0)
        {
            builder.Append('\\', backslashCount * 2);
        }

        builder.Append('"');
        return builder.ToString();
    }

    private static async Task WaitForPipeReadyAsync(
        Process process,
        string pipePath,
        ConcurrentQueue<string> startupOutput,
        CancellationToken cancellationToken)
    {
        var stopwatch = Stopwatch.StartNew();
        var timeout = TimeSpan.FromSeconds(10);

        while (stopwatch.Elapsed < timeout)
        {
            cancellationToken.ThrowIfCancellationRequested();

            if (process.HasExited)
            {
                throw new InvalidOperationException(BuildStartupFailureMessage(process.ExitCode, startupOutput));
            }

            if (WaitNamedPipe(pipePath, 100))
            {
                return;
            }

            await Task.Delay(50, cancellationToken).ConfigureAwait(false);
        }

        throw new TimeoutException("Timed out waiting for Librova core host pipe readiness.");
    }

    internal static string BuildStartupFailureMessage(int exitCode, IEnumerable<string> startupOutput)
    {
        var summary = string.Join(" ", startupOutput)
            .Trim();

        if (string.IsNullOrWhiteSpace(summary))
        {
            return $"Librova core host exited before pipe readiness. ExitCode={exitCode}.";
        }

        return $"Librova core host exited before pipe readiness. ExitCode={exitCode}. {summary}";
    }

    private static void AppendStartupOutput(ConcurrentQueue<string> startupOutput, string? line)
    {
        if (string.IsNullOrWhiteSpace(line))
        {
            return;
        }

        startupOutput.Enqueue(line.Trim());
        while (startupOutput.Count > 8 && startupOutput.TryDequeue(out _))
        {
        }
    }

    internal static void TryTerminate(Process process)
    {
        try
        {
            if (!process.HasExited)
            {
                process.Kill(entireProcessTree: true);
            }
        }
        catch (Exception error)
        {
            UiLogging.Warning(error, "Failed to terminate core host process.");
        }
    }

    private void CreateShutdownEvent(CoreHostLaunchOptions options)
    {
        if (!OperatingSystem.IsWindows() || string.IsNullOrWhiteSpace(options.ShutdownEventName))
        {
            return;
        }

        var eventHandle = SafeKernelHandle.CreateOwned(CreateEvent(IntPtr.Zero, true, false, options.ShutdownEventName));
        if (eventHandle.IsInvalid)
        {
            eventHandle.Dispose();
            throw new InvalidOperationException(
                $"Failed to create shutdown event for core host lifetime management. Error={Marshal.GetLastWin32Error()}.");
        }

        _shutdownEventHandle = eventHandle;
    }

    private async Task<bool> TryRequestGracefulShutdownAsync(Process process)
    {
        if (_shutdownEventHandle is null || _shutdownEventHandle.IsInvalid)
        {
            return false;
        }

        if (!SetEvent(_shutdownEventHandle))
        {
            UiLogging.Warning(
                "Failed to signal graceful shutdown for core host process. ProcessId={ProcessId} Error={ErrorCode}",
                process.Id,
                Marshal.GetLastWin32Error());
            return false;
        }

        try
        {
            using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(5));
            await process.WaitForExitAsync(timeout.Token).ConfigureAwait(false);
            UiLogging.Information("Core host process acknowledged graceful shutdown. ProcessId={ProcessId}", process.Id);
            return true;
        }
        catch (OperationCanceledException)
        {
            UiLogging.Warning(
                "Core host graceful shutdown timed out. Falling back to termination. ProcessId={ProcessId}",
                process.Id);
            return false;
        }
    }

    private void AttachProcessToLifetimeJob(Process process)
    {
        if (!OperatingSystem.IsWindows() || _jobHandle is not null)
        {
            return;
        }

        var jobHandle = SafeKernelHandle.CreateOwned(CreateJobObject(IntPtr.Zero, null));
        if (jobHandle.IsInvalid)
        {
            jobHandle.Dispose();
            UiLogging.Warning("Failed to create Windows job object for core host lifetime management. Error={ErrorCode}", Marshal.GetLastWin32Error());
            return;
        }

        var limitInformation = new JOBOBJECT_EXTENDED_LIMIT_INFORMATION
        {
            BasicLimitInformation = new JOBOBJECT_BASIC_LIMIT_INFORMATION
            {
                LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
            }
        };
        var length = Marshal.SizeOf<JOBOBJECT_EXTENDED_LIMIT_INFORMATION>();
        var pointer = Marshal.AllocHGlobal(length);

        try
        {
            Marshal.StructureToPtr(limitInformation, pointer, false);
            if (!SetInformationJobObject(jobHandle, JobObjectExtendedLimitInformation, pointer, (uint)length))
            {
                UiLogging.Warning("Failed to configure Windows job object for core host lifetime management. Error={ErrorCode}", Marshal.GetLastWin32Error());
                jobHandle.Dispose();
                return;
            }

            IntPtr processHandle;
            try
            {
                processHandle = process.Handle;
            }
            catch (InvalidOperationException error)
            {
                UiLogging.Warning(
                    error,
                    "Failed to access core host process handle for Windows job assignment.");
                jobHandle.Dispose();
                return;
            }

            if (!AssignProcessToJobObject(jobHandle, processHandle))
            {
                UiLogging.Warning("Failed to assign core host to Windows job object. ProcessId={ProcessId} Error={ErrorCode}", process.Id, Marshal.GetLastWin32Error());
                jobHandle.Dispose();
                return;
            }

            _jobHandle = jobHandle;
            UiLogging.Information(
                "Attached core host process to Windows lifetime job. ProcessId={ProcessId}",
                process.Id);
        }
        finally
        {
            Marshal.FreeHGlobal(pointer);
        }
    }

    private void ReleaseLifetimeJob()
    {
        if (_jobHandle is null)
        {
            return;
        }

        _jobHandle.Dispose();
        _jobHandle = null;
    }

    private void ReleaseShutdownEvent()
    {
        if (_shutdownEventHandle is null)
        {
            return;
        }

        _shutdownEventHandle.Dispose();
        _shutdownEventHandle = null;
    }

    private const uint JobObjectExtendedLimitInformation = 9;
    private const uint JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x00002000;

    [StructLayout(LayoutKind.Sequential)]
    private struct JOBOBJECT_BASIC_LIMIT_INFORMATION
    {
        public long PerProcessUserTimeLimit;
        public long PerJobUserTimeLimit;
        public uint LimitFlags;
        public UIntPtr MinimumWorkingSetSize;
        public UIntPtr MaximumWorkingSetSize;
        public uint ActiveProcessLimit;
        public UIntPtr Affinity;
        public uint PriorityClass;
        public uint SchedulingClass;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct IO_COUNTERS
    {
        public ulong ReadOperationCount;
        public ulong WriteOperationCount;
        public ulong OtherOperationCount;
        public ulong ReadTransferCount;
        public ulong WriteTransferCount;
        public ulong OtherTransferCount;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct JOBOBJECT_EXTENDED_LIMIT_INFORMATION
    {
        public JOBOBJECT_BASIC_LIMIT_INFORMATION BasicLimitInformation;
        public IO_COUNTERS IoInfo;
        public UIntPtr ProcessMemoryLimit;
        public UIntPtr JobMemoryLimit;
        public UIntPtr PeakProcessMemoryUsed;
        public UIntPtr PeakJobMemoryUsed;
    }

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern IntPtr CreateJobObject(IntPtr jobAttributes, string? name);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool SetInformationJobObject(SafeHandle job, uint infoType, IntPtr jobObjectInfo, uint jobObjectInfoLength);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool AssignProcessToJobObject(SafeHandle job, IntPtr process);

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool WaitNamedPipe(string name, int timeout);

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern IntPtr CreateEvent(IntPtr eventAttributes, bool manualReset, bool initialState, string name);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool SetEvent(SafeHandle handle);
}

internal class SafeKernelHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    protected SafeKernelHandle()
        : base(ownsHandle: true)
    {
    }

    internal static SafeKernelHandle CreateOwned(IntPtr handle)
    {
        var ownedHandle = new SafeKernelHandle();
        ownedHandle.SetHandle(handle);
        return ownedHandle;
    }

    protected override bool ReleaseHandle() => CloseHandle(handle);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool CloseHandle(IntPtr handle);
}
