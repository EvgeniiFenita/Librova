using Serilog;
using Serilog.Events;
using System;
using System.IO;

namespace LibriFlow.UI.Logging;

internal static class UiLogging
{
    private static readonly object SyncRoot = new();
    private static bool _initialized;
    private static string? _currentLogFilePath;

    public static string DefaultLogFilePath =>
        Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "LibriFlow",
            "Logs",
            "ui.log");

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
            ShutdownUnsafe();
            InitializeUnsafe(logFilePath);
        }
    }

    public static void Shutdown()
    {
        lock (SyncRoot)
        {
            ShutdownUnsafe();
        }
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
            .Enrich.WithProperty("Component", "LibriFlow.UI")
            .WriteTo.Debug(outputTemplate:
                "[{Timestamp:HH:mm:ss} {Level:u3}] {Message:lj}{NewLine}{Exception}")
            .WriteTo.File(
                path: logFilePath,
                rollingInterval: RollingInterval.Infinite,
                retainedFileCountLimit: 7,
                shared: true,
                outputTemplate:
                "{Timestamp:yyyy-MM-dd HH:mm:ss.fff zzz} [{Level:u3}] {Message:lj}{NewLine}{Exception}")
            .CreateLogger();

        _currentLogFilePath = logFilePath;
        _initialized = true;
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
