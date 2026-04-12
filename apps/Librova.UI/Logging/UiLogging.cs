using Serilog;
using Serilog.Events;
using System;
using System.IO;
using Librova.UI.Runtime;

namespace Librova.UI.Logging;

internal static class UiLogging
{
    private static readonly object SyncRoot = new();
    private static bool _initialized;
    private static string? _currentLogFilePath;

    public static string DefaultLogFilePath =>
        RuntimeEnvironment.GetDefaultUiLogFilePath();

    public static string? CurrentLogFilePath
    {
        get
        {
            lock (SyncRoot)
            {
                return _currentLogFilePath;
            }
        }
    }

    public static void EnsureInitialized()
    {
        lock (SyncRoot)
        {
            if (_initialized)
            {
                return;
            }

            InitializeUnsafe(DefaultLogFilePath);
        }
    }

    public static void ReinitializeForTests(string logFilePath)
    {
        lock (SyncRoot)
        {
            ReinitializeUnsafe(logFilePath, mergePreviousLog: false);
        }
    }

    public static void Reinitialize(string logFilePath)
    {
        lock (SyncRoot)
        {
            ReinitializeUnsafe(logFilePath, mergePreviousLog: true);
        }
    }

    public static void Shutdown()
    {
        lock (SyncRoot)
        {
            ShutdownUnsafe();
        }
    }

    public static void Debug(string messageTemplate, params object[] propertyValues)
    {
        EnsureInitialized();
        Log.Debug(messageTemplate, propertyValues);
    }

    public static void Information(string messageTemplate, params object[] propertyValues)
    {
        EnsureInitialized();
        Log.Information(messageTemplate, propertyValues);
    }

    public static void Warning(string messageTemplate, params object[] propertyValues)
    {
        EnsureInitialized();
        Log.Warning(messageTemplate, propertyValues);
    }

    public static void Warning(Exception exception, string messageTemplate, params object[] propertyValues)
    {
        EnsureInitialized();
        Log.Warning(exception, messageTemplate, propertyValues);
    }

    public static void Error(Exception exception, string messageTemplate, params object[] propertyValues)
    {
        EnsureInitialized();
        Log.Error(exception, messageTemplate, propertyValues);
    }

    private static void InitializeUnsafe(string logFilePath)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(logFilePath)!);

        Log.Logger = new LoggerConfiguration()
            .MinimumLevel.Debug()
            .Enrich.WithProperty("Component", "Librova.UI")
            .WriteTo.Debug(outputTemplate:
                "[{Timestamp:HH:mm:ss} {Level:u3}] {Message:lj}{NewLine}{Exception}")
            .WriteTo.File(
                path: logFilePath,
                rollingInterval: RollingInterval.Infinite,
                retainedFileCountLimit: 7,
                shared: true,
                restrictedToMinimumLevel: LogEventLevel.Information,
                outputTemplate:
                "{Timestamp:yyyy-MM-dd HH:mm:ss.fff zzz} [{Level:u3}] {Message:lj}{NewLine}{Exception}")
            .CreateLogger();

        _currentLogFilePath = logFilePath;
        _initialized = true;
    }

    private static void ReinitializeUnsafe(string logFilePath, bool mergePreviousLog)
    {
        if (_initialized && string.Equals(_currentLogFilePath, logFilePath, StringComparison.OrdinalIgnoreCase))
        {
            return;
        }

        var previousLogFilePath = _currentLogFilePath;
        ShutdownUnsafe();

        if (mergePreviousLog
            && !string.IsNullOrWhiteSpace(previousLogFilePath)
            && !string.Equals(previousLogFilePath, logFilePath, StringComparison.OrdinalIgnoreCase)
            && File.Exists(previousLogFilePath))
        {
            Directory.CreateDirectory(Path.GetDirectoryName(logFilePath)!);

            using (var source = new FileStream(previousLogFilePath, FileMode.Open, FileAccess.Read, FileShare.Read))
            using (var dest = new FileStream(logFilePath, FileMode.Append, FileAccess.Write, FileShare.Read))
            {
                source.CopyTo(dest);
            }

            try
            {
                File.Delete(previousLogFilePath);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }

        InitializeUnsafe(logFilePath);
    }

    private static void ShutdownUnsafe()
    {
        if (_initialized)
        {
            Log.CloseAndFlush();
        }

        _initialized = false;
        _currentLogFilePath = null;
    }
}
