using Librova.UI.CoreHost;
using Librova.UI.ImportJobs;
using Librova.UI.LibraryCatalog;
using Xunit;

namespace Librova.UI.Tests;

public sealed class LibraryCatalogServiceTests
{
    [Fact]
    public async Task LibraryCatalogService_ListsBooksWithoutGeneratedProtoTypes()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-library-service",
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
                PipePath = $@"\\.\pipe\Librova.UI.LibraryServiceTests.{Environment.ProcessId}.{Environment.TickCount64}",
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
                      <book-title>Monday Begins on Saturday</book-title>
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

            var service = new LibraryCatalogService(options.PipePath);
            var page = await service.ListBooksAsync(
                new BookListRequestModel
                {
                    Text = "monday",
                    SortBy = BookSortModel.DateAdded,
                    Limit = 10
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Single(page.Items);
            Assert.Equal(1UL, page.TotalCount);
            Assert.Equal("Monday Begins on Saturday", page.Items[0].Title);
            Assert.Equal(BookFormatModel.Fb2, page.Items[0].Format);
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
    public async Task LibraryCatalogService_ExportsBookWithoutGeneratedProtoTypes()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-library-service-export",
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
                PipePath = $@"\\.\pipe\Librova.UI.LibraryServiceExportTests.{Environment.ProcessId}.{Environment.TickCount64}",
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
                      <book-title>Export Through Service</book-title>
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

            var service = new LibraryCatalogService(options.PipePath);
            var page = await service.ListBooksAsync(
                new BookListRequestModel
                {
                    Text = "export",
                    Limit = 10
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Single(page.Items);
            Assert.Equal(1UL, page.TotalCount);

            var exportPath = Path.Combine(sandboxRoot, "Exports", "ExportThroughService.fb2");
            var exportedPath = await service.ExportBookAsync(
                page.Items[0].BookId,
                exportPath,
                null,
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Equal(Path.GetFullPath(exportPath), Path.GetFullPath(exportedPath!));
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
    public async Task LibraryCatalogService_LoadsAggregateLibraryStatisticsWithoutGeneratedProtoTypes()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-library-service-statistics",
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
                PipePath = $@"\\.\pipe\Librova.UI.LibraryServiceStatisticsTests.{Environment.ProcessId}.{Environment.TickCount64}",
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
                      <book-title>Statistics Through Service</book-title>
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

            var service = new LibraryCatalogService(options.PipePath);
            var page = await service.ListBooksAsync(
                new BookListRequestModel
                {
                    Limit = 10
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);
            var statistics = await service.GetLibraryStatisticsAsync(
                TimeSpan.FromSeconds(5),
                cancellation.Token);
            var expectedManagedBookSizeBytes = page.Items
                .Select(item => ResolveLibraryPath(options.LibraryRoot, item.ManagedPath))
                .Aggregate(0UL, (total, path) => total + (ulong)new FileInfo(path).Length);
            var databasePath = Path.Combine(options.LibraryRoot, "Database", "librova.db");
            var databaseSizeBytes = (ulong)new FileInfo(databasePath).Length;

            Assert.Equal(1UL, statistics.BookCount);
            Assert.Single(page.Items);
            Assert.Equal(1UL, page.TotalCount);
            Assert.True(databaseSizeBytes > 0);
            Assert.Equal(expectedManagedBookSizeBytes, statistics.TotalManagedBookSizeBytes);
            Assert.NotEqual(expectedManagedBookSizeBytes + databaseSizeBytes, statistics.TotalManagedBookSizeBytes);
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
    public async Task LibraryCatalogService_ExportsFb2AsEpubWhenConverterIsConfigured()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-library-service-export-converted",
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
                PipePath = $@"\\.\pipe\Librova.UI.LibraryServiceExportConvertedTests.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library"),
                LibraryOpenMode = UiLibraryOpenMode.CreateNew,
                ConverterMode = UiConverterMode.CustomCommand,
                CustomConverterExecutablePath = @"C:\Program Files\PowerShell\7\pwsh.exe",
                CustomConverterArguments = ["-File", scriptPath, "{source}", "{destination}"],
                CustomConverterOutputMode = UiConverterOutputMode.ExactDestinationPath
            };

            var sourcePath = Path.Combine(sandboxRoot, "book.fb2");
            await File.WriteAllTextAsync(sourcePath,
                """
                <?xml version="1.0" encoding="UTF-8"?>
                <FictionBook>
                  <description>
                    <title-info>
                      <book-title>Export As EPUB Through Service</book-title>
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

            var service = new LibraryCatalogService(options.PipePath);
            var page = await service.ListBooksAsync(
                new BookListRequestModel
                {
                    Text = "export as epub",
                    Limit = 10
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Single(page.Items);
            Assert.Equal(1UL, page.TotalCount);

            var exportPath = Path.Combine(sandboxRoot, "Exports", "ExportAsEpubThroughService.epub");
            var exportedPath = await service.ExportBookAsync(
                page.Items[0].BookId,
                exportPath,
                BookFormatModel.Epub,
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Equal(Path.GetFullPath(exportPath), Path.GetFullPath(exportedPath!));
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
    public async Task LibraryCatalogService_MovesBookToTrashWithoutGeneratedProtoTypes()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-library-service-trash",
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
                PipePath = $@"\\.\pipe\Librova.UI.LibraryServiceTrashTests.{Environment.ProcessId}.{Environment.TickCount64}",
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
                      <book-title>Trash Through Service</book-title>
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

            var service = new LibraryCatalogService(options.PipePath);
            var page = await service.ListBooksAsync(
                new BookListRequestModel
                {
                    Text = "trash",
                    Limit = 10
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Single(page.Items);
            Assert.Equal(1UL, page.TotalCount);

            var moved = await service.MoveBookToTrashAsync(
                page.Items[0].BookId,
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.True(moved);

            var afterDelete = await service.ListBooksAsync(
                new BookListRequestModel
                {
                    Text = "trash",
                    Limit = 10
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Empty(afterDelete.Items);
            Assert.Equal(0UL, afterDelete.TotalCount);
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
