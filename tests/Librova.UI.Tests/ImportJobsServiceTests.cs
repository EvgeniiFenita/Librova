using Librova.UI.CoreHost;
using Librova.UI.ImportJobs;
using Xunit;

namespace Librova.UI.Tests;

public sealed class ImportJobsServiceTests
{
    [Fact]
    public async Task Service_PerformsEndToEndImportFlowWithoutGeneratedProtoTypes()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-import-service",
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
                PipePath = $@"\\.\pipe\Librova.UI.ServiceTests.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library")
            };

            var sourcePath = Path.Combine(sandboxRoot, "book.fb2");
            await File.WriteAllTextAsync(sourcePath,
                """
                <?xml version="1.0" encoding="UTF-8"?>
                <FictionBook xmlns:l="http://www.w3.org/1999/xlink">
                  <description>
                    <title-info>
                      <book-title>Понедельник начинается в субботу</book-title>
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

            var service = new ImportJobsService(options.PipePath);
            var jobId = await service.StartAsync(
                new StartImportRequestModel
                {
                    SourcePath = sourcePath,
                    WorkingDirectory = Path.Combine(sandboxRoot, "Work")
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.NotEqual(0UL, jobId);
            Assert.True(await service.WaitAsync(
                jobId,
                TimeSpan.FromSeconds(5),
                TimeSpan.FromSeconds(2),
                cancellation.Token));

            var result = await service.TryGetResultAsync(jobId, TimeSpan.FromSeconds(5), cancellation.Token);
            Assert.NotNull(result);
            Assert.Equal(ImportJobStatusModel.Completed, result!.Snapshot.Status);
            Assert.Equal(1UL, result.Summary!.ImportedEntries);

            Assert.True(await service.RemoveAsync(jobId, TimeSpan.FromSeconds(5), cancellation.Token));
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
