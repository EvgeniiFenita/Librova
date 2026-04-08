using Librova.UI.CoreHost;
using Librova.UI.LibraryCatalog;
using Librova.UI.ImportJobs;
using Librova.V1;
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
                LibraryRoot = Path.Combine(sandboxRoot, "Library"),
                LibraryOpenMode = UiLibraryOpenMode.CreateNew
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
                    SourcePaths = [sourcePath],
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
                LibraryRoot = Path.Combine(sandboxRoot, "Library"),
                LibraryOpenMode = UiLibraryOpenMode.CreateNew
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
                    SourcePaths = [sourcePath],
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

    [Fact]
    public async Task LibraryCatalogClient_LoadsAggregateLibraryStatisticsThroughNativeHost()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-library-client-statistics",
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
                PipePath = $@"\\.\pipe\Librova.UI.LibraryStatisticsTests.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library"),
                LibraryOpenMode = UiLibraryOpenMode.CreateNew
            };

            var sourceOnePath = Path.Combine(sandboxRoot, "one.fb2");
            var sourceTwoPath = Path.Combine(sandboxRoot, "two.fb2");
            await File.WriteAllTextAsync(sourceOnePath,
                """
                <?xml version="1.0" encoding="UTF-8"?>
                <FictionBook><description><title-info><book-title>One</book-title><author><first-name>A</first-name><last-name>One</last-name></author><lang>en</lang></title-info></description><body><section><p>Body One</p></section></body></FictionBook>
                """);
            await File.WriteAllTextAsync(sourceTwoPath,
                """
                <?xml version="1.0" encoding="UTF-8"?>
                <FictionBook><description><title-info><book-title>Two</book-title><author><first-name>B</first-name><last-name>Two</last-name></author><lang>en</lang></title-info></description><body><section><p>Body Two</p></section></body></FictionBook>
                """);

            await using var process = new CoreHostProcess();
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(20));
            await process.StartAsync(options, cancellation.Token);

            var importService = new ImportJobsService(options.PipePath);
            foreach (var sourcePath in new[] { sourceOnePath, sourceTwoPath })
            {
                var jobId = await importService.StartAsync(
                    new StartImportRequestModel
                    {
                        SourcePaths = [sourcePath],
                        WorkingDirectory = Path.Combine(sandboxRoot, "Work")
                    },
                    TimeSpan.FromSeconds(5),
                    cancellation.Token);

                Assert.True(await importService.WaitAsync(
                    jobId,
                    TimeSpan.FromSeconds(5),
                    TimeSpan.FromSeconds(2),
                    cancellation.Token));
            }

            var client = new LibraryCatalogClient(options.PipePath);
            var listResponse = await client.ListBooksAsync(
                LibraryCatalogMapper.ToProto(new BookListRequestModel
                {
                    Limit = 10
                }),
                TimeSpan.FromSeconds(5),
                cancellation.Token);
            var coversRoot = Path.Combine(options.LibraryRoot, "Covers");
            Directory.CreateDirectory(coversRoot);
            var coverPath = Path.Combine(coversRoot, "manual-cover.png");
            await File.WriteAllTextAsync(coverPath, "stub-cover-file");
            var coverSizeBytes = (ulong)new FileInfo(coverPath).Length;
            var response = await client.GetLibraryStatisticsAsync(
                TimeSpan.FromSeconds(5),
                cancellation.Token);
            var expectedManagedBookSizeBytes = listResponse.Items
                .Select(item => ResolveLibraryPath(options.LibraryRoot, item.ManagedPath))
                .Aggregate(0UL, (total, path) => total + (ulong)new FileInfo(path).Length);
            var databasePath = Path.Combine(options.LibraryRoot, "Database", "librova.db");
            var databaseSizeBytes = (ulong)new FileInfo(databasePath).Length;

            Assert.NotNull(response.Statistics);
            Assert.Equal(2UL, response.Statistics.BookCount);
            Assert.Equal(2, listResponse.Items.Count);
            Assert.True(databaseSizeBytes > 0);
            Assert.Equal(expectedManagedBookSizeBytes + coverSizeBytes + databaseSizeBytes, response.Statistics.TotalLibrarySizeBytes);
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
    public async Task LibraryCatalogClient_ExportsSelectedBookThroughNativeHost()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-library-client-export",
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
                PipePath = $@"\\.\pipe\Librova.UI.LibraryExportTests.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library"),
                LibraryOpenMode = UiLibraryOpenMode.CreateNew
            };

            var sourcePath = Path.Combine(sandboxRoot, "book.fb2");
            await File.WriteAllTextAsync(sourcePath,
                """
                <?xml version="1.0" encoding="UTF-8"?>
                <FictionBook>
                  <description>
                    <title-info>
                      <book-title>Export Me</book-title>
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
                    SourcePaths = [sourcePath],
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
            var listResponse = await client.ListBooksAsync(
                LibraryCatalogMapper.ToProto(new BookListRequestModel
                {
                    Text = "export",
                    Limit = 10
                }),
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Single(listResponse.Items);

            var exportPath = Path.Combine(sandboxRoot, "Exports", "ExportMe.fb2");
            var exportResponse = await client.ExportBookAsync(
                listResponse.Items[0].BookId,
                exportPath,
                null,
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.True(exportResponse.HasExportedPath);
            Assert.Equal(Path.GetFullPath(exportPath), Path.GetFullPath(exportResponse.ExportedPath));
            Assert.True(File.Exists(exportPath));
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
    public async Task LibraryCatalogClient_RejectsFb2AsEpubExportWhenNoConverterIsConfigured()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-library-client-export-converted",
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var scriptPath = Path.Combine(sandboxRoot, "converter.ps1");
            await File.WriteAllTextAsync(scriptPath,
                """
                param(
                  [string]$source,
                  [string]$destination
                )
                if ($source -notmatch '[\\/]+Library[\\/]+Books[\\/]+')
                {
                  Write-Error 'Import conversion intentionally disabled for this test.'
                  exit 1
                }
                Copy-Item -LiteralPath $source -Destination $destination -Force
                """);

            var options = new CoreHostLaunchOptions
            {
                ExecutablePath = CoreHostPathResolver.ResolveDevelopmentExecutablePath(Path.Combine(
                    AppContext.BaseDirectory,
                    "..",
                    "..",
                    "..",
                    "..")),
                PipePath = $@"\\.\pipe\Librova.UI.LibraryExportConvertedTests.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library"),
                LibraryOpenMode = UiLibraryOpenMode.CreateNew
            };

            var sourcePath = Path.Combine(sandboxRoot, "book.fb2");
            await File.WriteAllTextAsync(sourcePath,
                """
                <?xml version="1.0" encoding="UTF-8"?>
                <FictionBook>
                  <description>
                    <title-info>
                      <book-title>Export Me As EPUB</book-title>
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
                    SourcePaths = [sourcePath],
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
            var listResponse = await client.ListBooksAsync(
                LibraryCatalogMapper.ToProto(new BookListRequestModel
                {
                    Text = "export me as epub",
                    Limit = 10
                }),
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Single(listResponse.Items);
            Assert.Equal(BookFormat.Fb2, listResponse.Items[0].Format);

            var exportPath = Path.Combine(sandboxRoot, "Exports", "ExportMeAsEpub.epub");
            var error = await Assert.ThrowsAsync<Librova.UI.PipeTransport.PipeTransportException>(async () =>
                await client.ExportBookAsync(
                    listResponse.Items[0].BookId,
                    exportPath,
                    BookFormatModel.Epub,
                    TimeSpan.FromSeconds(5),
                    cancellation.Token));
            Assert.Contains("converter is unavailable", error.Message, StringComparison.OrdinalIgnoreCase);
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
    public async Task LibraryCatalogClient_MovesSelectedBookToTrashThroughNativeHost()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-library-client-trash",
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
                PipePath = $@"\\.\pipe\Librova.UI.LibraryTrashTests.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library"),
                LibraryOpenMode = UiLibraryOpenMode.CreateNew
            };

            var sourcePath = Path.Combine(sandboxRoot, "book.fb2");
            await File.WriteAllTextAsync(sourcePath,
                """
                <?xml version="1.0" encoding="UTF-8"?>
                <FictionBook>
                  <description>
                    <title-info>
                      <book-title>Trash Me</book-title>
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
                    SourcePaths = [sourcePath],
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
            var listResponse = await client.ListBooksAsync(
                LibraryCatalogMapper.ToProto(new BookListRequestModel
                {
                    Text = "trash",
                    Limit = 10
                }),
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Single(listResponse.Items);

            var trashResponse = await client.MoveBookToTrashAsync(
                listResponse.Items[0].BookId,
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.True(trashResponse.HasTrashedBookId);
            Assert.Equal(listResponse.Items[0].BookId, trashResponse.TrashedBookId);
            Assert.Equal(DeleteDestination.RecycleBin, trashResponse.Destination);
            Assert.False(trashResponse.HasOrphanedFiles);
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

    private static string ResolveLibraryPath(string libraryRoot, string managedPath) =>
        Path.IsPathFullyQualified(managedPath)
            ? managedPath
            : Path.GetFullPath(Path.Combine(libraryRoot, managedPath));
}
