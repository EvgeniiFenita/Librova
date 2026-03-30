using Librova.V1;
using Librova.UI.CoreHost;
using Librova.UI.ImportJobs;
using Xunit;

namespace Librova.UI.Tests;

public sealed class ImportJobClientTests
{
    [Fact]
    public async Task ImportJobClient_PerformsEndToEndStartWaitResultAndRemove()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-import-client",
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
                PipePath = $@"\\.\pipe\Librova.UI.ImportTests.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library")
            };

            var sourcePath = Path.Combine(sandboxRoot, "book.fb2");
            await File.WriteAllTextAsync(sourcePath,
                """
                <?xml version="1.0" encoding="UTF-8"?>
                <FictionBook xmlns:l="http://www.w3.org/1999/xlink">
                  <description>
                    <title-info>
                      <book-title>Пикник на обочине</book-title>
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

            await using var process = new CoreHostProcess();
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(20));
            await process.StartAsync(options, cancellation.Token);

            var client = new ImportJobClient(options.PipePath);
            var jobId = await client.StartImportAsync(
                new ImportRequest
                {
                    SourcePath = sourcePath,
                    WorkingDirectory = Path.Combine(sandboxRoot, "Work")
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.NotEqual(0UL, jobId);

            var waitResponse = await client.WaitAsync(
                jobId,
                TimeSpan.FromSeconds(5),
                TimeSpan.FromSeconds(2),
                cancellation.Token);

            Assert.True(waitResponse.Completed);

            var resultResponse = await client.GetResultAsync(jobId, TimeSpan.FromSeconds(5), cancellation.Token);
            Assert.NotNull(resultResponse.Result);
            Assert.Equal(ImportJobStatus.Completed, resultResponse.Result.Snapshot.Status);
            Assert.Equal(1UL, resultResponse.Result.Summary.ImportedEntries);

            var removeResponse = await client.RemoveAsync(jobId, TimeSpan.FromSeconds(5), cancellation.Token);
            Assert.True(removeResponse.Removed);
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
