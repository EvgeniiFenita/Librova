using Librova.UI.CoreHost;
using Librova.UI.ImportJobs;
using Librova.UI.Shell;
using Xunit;

namespace Librova.UI.Tests;

public sealed class ShellBootstrapTests
{
    [Fact]
    public async Task ShellBootstrap_StartsSessionAndProvidesUiFacingImportService()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-shell-bootstrap",
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
                PipePath = $@"\\.\pipe\Librova.UI.ShellTests.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library")
            };

            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(20));
            await using var session = await ShellBootstrap.StartSessionAsync(options, cancellation.Token);

            Assert.Equal(options.PipePath, session.HostOptions.PipePath);
            Assert.NotNull(session.ImportJobs);
            Assert.True(Directory.Exists(Path.Combine(options.LibraryRoot, "Logs")));

            var sourcePath = Path.Combine(sandboxRoot, "book.fb2");
            await File.WriteAllTextAsync(sourcePath,
                """
                <?xml version="1.0" encoding="UTF-8"?>
                <FictionBook xmlns:l="http://www.w3.org/1999/xlink">
                  <description>
                    <title-info>
                      <book-title>Хищные вещи века</book-title>
                      <author>
                        <first-name>Аркадий</first-name>
                        <last-name>Стругацкий</last-name>
                      </author>
                      <lang>ru</lang>
                    </title-info>
                  </description>
                  <body>
                    <section><p>Body</p></section>
                  </body>
                </FictionBook>
                """);

            var jobId = await session.ImportJobs.StartAsync(
                new StartImportRequestModel
                {
                    SourcePath = sourcePath,
                    WorkingDirectory = Path.Combine(sandboxRoot, "Work")
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.True(await session.ImportJobs.WaitAsync(
                jobId,
                TimeSpan.FromSeconds(5),
                TimeSpan.FromSeconds(2),
                cancellation.Token));

            var result = await session.ImportJobs.TryGetResultAsync(jobId, TimeSpan.FromSeconds(5), cancellation.Token);
            Assert.NotNull(result);
            Assert.Equal(ImportJobStatusModel.Completed, result!.Snapshot.Status);
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
}
