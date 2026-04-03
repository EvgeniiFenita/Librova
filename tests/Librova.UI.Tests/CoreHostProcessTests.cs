using Librova.UI.CoreHost;
using Xunit;

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
            ParentProcessId = 12345,
            ConverterMode = UiConverterMode.BuiltInFb2Cng,
            Fb2CngExecutablePath = @"C:\Tools\fbc.exe",
            Fb2CngConfigPath = @"C:\Tools\fbc.yaml"
        };

        var arguments = CoreHostProcess.BuildArguments(options);

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
            ConverterMode = UiConverterMode.CustomCommand,
            CustomConverterExecutablePath = @"C:\Tools\custom.exe",
            CustomConverterArguments = ["--input", "{source}", "--output-dir", "{destination_dir}"],
            CustomConverterOutputMode = UiConverterOutputMode.SingleFileInDestinationDirectory
        };

        var arguments = CoreHostProcess.BuildArguments(options);

        Assert.Contains("--library-mode open", arguments, StringComparison.Ordinal);
        Assert.Contains("--converter-exe", arguments, StringComparison.Ordinal);
        Assert.Contains(@"""C:\Tools\custom.exe""", arguments, StringComparison.Ordinal);
        Assert.Contains("--converter-arg", arguments, StringComparison.Ordinal);
        Assert.Contains(@"""{source}""", arguments, StringComparison.Ordinal);
        Assert.Contains("--converter-output directory", arguments, StringComparison.Ordinal);
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
}
