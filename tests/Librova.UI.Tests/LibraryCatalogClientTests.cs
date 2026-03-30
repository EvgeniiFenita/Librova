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

    [Fact]
    public async Task LibraryCatalogClient_LoadsBookDetailsThroughNativeHost()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-library-client-details",
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
                PipePath = $@"\\.\pipe\Librova.UI.LibraryDetailsTests.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library")
            };

            var sourcePath = Path.Combine(sandboxRoot, "book.fb2");
            await File.WriteAllTextAsync(sourcePath,
                """
                <?xml version="1.0" encoding="UTF-8"?>
                <FictionBook xmlns:l="http://www.w3.org/1999/xlink">
                  <description>
                    <title-info>
                      <book-title>Definitely Maybe</book-title>
                      <author>
                        <first-name>Arkady</first-name>
                        <last-name>Strugatsky</last-name>
                      </author>
                      <lang>en</lang>
                      <annotation><p>Aliens land only in one city.</p></annotation>
                    </title-info>
                    <publish-info>
                      <book-name>Definitely Maybe</book-name>
                      <publisher>Macmillan</publisher>
                      <year>1978</year>
                      <isbn>978-5-17-000000-1</isbn>
                    </publish-info>
                    <document-info>
                      <id>details-id-1</id>
                    </document-info>
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

            var listClient = new LibraryCatalogClient(options.PipePath);
            var listResponse = await listClient.ListBooksAsync(
                LibraryCatalogMapper.ToProto(new BookListRequestModel
                {
                    Text = "definitely",
                    Limit = 10
                }),
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Single(listResponse.Items);
            var detailsResponse = await listClient.GetBookDetailsAsync(
                listResponse.Items[0].BookId,
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.NotNull(detailsResponse.Details);
            Assert.Equal("Definitely Maybe", detailsResponse.Details.Title);
            Assert.Equal("Macmillan", detailsResponse.Details.Publisher);
            Assert.Equal("9785170000001", detailsResponse.Details.Isbn);
            Assert.Equal("Aliens land only in one city.", detailsResponse.Details.Description);
            Assert.Equal("details-id-1", detailsResponse.Details.Identifier);
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
