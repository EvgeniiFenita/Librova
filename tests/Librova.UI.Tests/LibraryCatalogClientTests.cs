using Librova.UI.CoreHost;
using Librova.UI.LibraryCatalog;
using Librova.UI.ImportJobs;
using Xunit;

namespace Librova.UI.Tests;

public sealed class LibraryCatalogClientTests
{
    [Fact]
    public async Task LibraryCatalogClient_ListsImportedBooksThroughNativeHost()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-library-client",
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
                PipePath = $@"\\.\pipe\Librova.UI.LibraryClientTests.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library")
            };

            var sourcePath = Path.Combine(sandboxRoot, "book.fb2");
            await File.WriteAllTextAsync(sourcePath,
                """
                <?xml version="1.0" encoding="UTF-8"?>
                <FictionBook xmlns:l="http://www.w3.org/1999/xlink">
                  <description>
                    <title-info>
                      <book-title>Roadside Picnic</book-title>
                      <author>
                        <first-name>Arkady</first-name>
                        <last-name>Strugatsky</last-name>
                      </author>
                      <lang>en</lang>
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

            var importService = new ImportJobsService(options.PipePath);
            var jobId = await importService.StartAsync(
                new StartImportRequestModel
                {
                    SourcePath = sourcePath,
                    WorkingDirectory = Path.Combine(sandboxRoot, "Work")
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.True(await importService.WaitAsync(
                jobId,
                TimeSpan.FromSeconds(5),
                TimeSpan.FromSeconds(2),
                cancellation.Token));

            var client = new LibraryCatalogClient(options.PipePath);
            var response = await client.ListBooksAsync(
                LibraryCatalogMapper.ToProto(new BookListRequestModel
                {
                    Text = "road",
                    Limit = 10
                }),
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Single(response.Items);
            Assert.Equal("Roadside Picnic", response.Items[0].Title);
            Assert.Equal("Arkady Strugatsky", response.Items[0].Authors[0]);
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
