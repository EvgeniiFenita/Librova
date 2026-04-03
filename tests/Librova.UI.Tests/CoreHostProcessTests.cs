using Librova.UI.CoreHost;
using Xunit;
using System.Runtime.InteropServices;

namespace Librova.UI.Tests;

public sealed class CoreHostProcessTests
{
    [Fact]
    public void BuildArguments_IncludesBuiltInConverterOptions()
    {
        var options = new CoreHostLaunchOptions
        {
            ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
            PipePath = @"\\.\pipe\Librova.Test",
            LibraryRoot = @"C:\Libraries\Librova",
            ShutdownEventName = @"Local\Librova.UI.Tests.Shutdown",
            ParentProcessId = 12345,
            ConverterMode = UiConverterMode.BuiltInFb2Cng,
            Fb2CngExecutablePath = @"C:\Tools\fbc.exe",
            Fb2CngConfigPath = @"C:\Tools\fbc.yaml"
        };

        var arguments = CoreHostProcess.BuildArguments(options);

        Assert.Contains("--shutdown-event", arguments, StringComparison.Ordinal);
        Assert.Contains(@"""Local\Librova.UI.Tests.Shutdown""", arguments, StringComparison.Ordinal);
        Assert.Contains("--library-mode open", arguments, StringComparison.Ordinal);
        Assert.Contains("--fb2cng-exe", arguments, StringComparison.Ordinal);
        Assert.Contains(@"""C:\Tools\fbc.exe""", arguments, StringComparison.Ordinal);
        Assert.Contains("--fb2cng-config", arguments, StringComparison.Ordinal);
        Assert.Contains("--parent-pid 12345", arguments, StringComparison.Ordinal);
    }

    [Fact]
    public void BuildArguments_IncludesCustomConverterOptions()
    {
        var options = new CoreHostLaunchOptions
        {
            ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
            PipePath = @"\\.\pipe\Librova.Test",
            LibraryRoot = @"C:\Libraries\Librova",
            ShutdownEventName = @"Local\Librova.UI.Tests.Shutdown",
            ConverterMode = UiConverterMode.CustomCommand,
            CustomConverterExecutablePath = @"C:\Tools\custom.exe",
            CustomConverterArguments = ["--input", "{source}", "--output-dir", "{destination_dir}"],
            CustomConverterOutputMode = UiConverterOutputMode.SingleFileInDestinationDirectory
        };

        var arguments = CoreHostProcess.BuildArguments(options);

        Assert.Contains("--shutdown-event", arguments, StringComparison.Ordinal);
        Assert.Contains("--library-mode open", arguments, StringComparison.Ordinal);
        Assert.Contains("--converter-exe", arguments, StringComparison.Ordinal);
        Assert.Contains(@"""C:\Tools\custom.exe""", arguments, StringComparison.Ordinal);
        Assert.Contains("--converter-arg", arguments, StringComparison.Ordinal);
        Assert.Contains(@"""{source}""", arguments, StringComparison.Ordinal);
        Assert.Contains("--converter-output directory", arguments, StringComparison.Ordinal);
    }

    [Fact]
    public void BuildArguments_EscapesTrailingBackslashesEmbeddedQuotesAndCyrillicForCommandLineToArgv()
    {
        var options = new CoreHostLaunchOptions
        {
            ExecutablePath = @"C:\Tools\LibrovaCoreHostApp.exe",
            PipePath = @"\\.\pipe\Librova ""quoted"" pipe\",
            LibraryRoot = @"C:\Книги\Новая папка\",
            ShutdownEventName = @"Local\Librova ""shutdown""\",
            ConverterMode = UiConverterMode.CustomCommand,
            CustomConverterExecutablePath = @"C:\Tools\conv ""special""\",
            CustomConverterArguments =
            [
                @"--template=""{source}""",
                @"C:\вход\folder\"
            ],
            CustomConverterOutputMode = UiConverterOutputMode.ExactDestinationPath
        };

        var arguments = CoreHostProcess.BuildArguments(options);
        var parsedArguments = ParseCommandLineArguments(arguments);

        Assert.Equal("--pipe", parsedArguments[0]);
        Assert.Equal(options.PipePath, parsedArguments[1]);
        Assert.Equal("--library-root", parsedArguments[2]);
        Assert.Equal(options.LibraryRoot, parsedArguments[3]);
        Assert.Equal("--shutdown-event", parsedArguments[4]);
        Assert.Equal(options.ShutdownEventName, parsedArguments[5]);
        Assert.Equal("--library-mode", parsedArguments[6]);
        Assert.Equal("open", parsedArguments[7]);
        Assert.Equal("--converter-exe", parsedArguments[8]);
        Assert.Equal(options.CustomConverterExecutablePath, parsedArguments[9]);
        Assert.Equal("--converter-arg", parsedArguments[10]);
        Assert.Equal(options.CustomConverterArguments[0], parsedArguments[11]);
        Assert.Equal("--converter-arg", parsedArguments[12]);
        Assert.Equal(options.CustomConverterArguments[1], parsedArguments[13]);
        Assert.Equal("--converter-output", parsedArguments[14]);
        Assert.Equal("exact", parsedArguments[15]);
    }

    [Fact]
    public async Task StartAsync_StartsCoreHostAndWaitsForPipeReadiness()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-tests",
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var options = new CoreHostLaunchOptions
            {
                ExecutablePath = CoreHostPathResolver.ResolveDevelopmentExecutablePath(Path.Combine(
                    AppContext.BaseDirectory,
                    "..",
                    "..",
                    "..",
                    "..")),
                PipePath = $@"\\.\pipe\Librova.UI.Tests.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library"),
                ShutdownEventName = $@"Local\Librova.UI.Tests.Shutdown.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryOpenMode = UiLibraryOpenMode.CreateNew,
                ConverterMode = UiConverterMode.Disabled
            };

            await using var process = new CoreHostProcess();
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(15));

            await process.StartAsync(options, cancellation.Token);

            Assert.True(Directory.Exists(Path.Combine(options.LibraryRoot, "Logs")));
            await process.DisposeAsync();

            var hostLogPath = Path.Combine(options.LibraryRoot, "Logs", "host.log");
            Assert.True(File.Exists(hostLogPath));

            var hostLogContents = await ReadAllTextWhenAvailableAsync(hostLogPath, TimeSpan.FromSeconds(5));
            Assert.DoesNotContain("Pipe session failed", hostLogContents, StringComparison.Ordinal);
            Assert.Contains("Shutdown requested by UI event", hostLogContents, StringComparison.Ordinal);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, recursive: true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    [Fact]
    public void BuildStartupFailureMessage_IncludesExitCodeAndCapturedOutput()
    {
        var message = CoreHostProcess.BuildStartupFailureMessage(
            1,
            [
                "Librova.Core.Host failed: Library root must be on an available drive."
            ]);

        Assert.Contains("ExitCode=1", message, StringComparison.Ordinal);
        Assert.Contains("Library root must be on an available drive.", message, StringComparison.Ordinal);
    }

    private static async Task<string> ReadAllTextWhenAvailableAsync(string path, TimeSpan timeout)
    {
        var deadline = DateTime.UtcNow + timeout;
        while (DateTime.UtcNow < deadline)
        {
            try
            {
                return await File.ReadAllTextAsync(path);
            }
            catch (IOException)
            {
                await Task.Delay(50);
            }
        }

        return await File.ReadAllTextAsync(path);
    }

    private static string[] ParseCommandLineArguments(string arguments)
    {
        var commandLine = $"Librova.Core.Host.exe {arguments}";
        var argv = CommandLineToArgvW(commandLine, out var argc);
        if (argv == IntPtr.Zero)
        {
            throw new InvalidOperationException($"CommandLineToArgvW failed with error {Marshal.GetLastWin32Error()}.");
        }

        try
        {
            var parsed = new string[argc - 1];
            for (var index = 1; index < argc; index++)
            {
                var current = Marshal.ReadIntPtr(argv, index * IntPtr.Size);
                parsed[index - 1] = Marshal.PtrToStringUni(current)
                    ?? throw new InvalidOperationException("CommandLineToArgvW returned a null argument.");
            }

            return parsed;
        }
        finally
        {
            LocalFree(argv);
        }
    }

    [DllImport("shell32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern IntPtr CommandLineToArgvW(string commandLine, out int argc);

    [DllImport("kernel32.dll")]
    private static extern IntPtr LocalFree(IntPtr handle);
}
